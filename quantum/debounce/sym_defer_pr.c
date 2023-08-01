/*
Copyright 2021 Chad Austin <chad@chadaustin.me>
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
Symmetric per-row debounce algorithm. Changes only apply when
DEBOUNCE milliseconds have elapsed since the last change.
*/

#include "debounce.h"
#include "timer.h"
#include <stdlib.h>

#ifdef PROTOCOL_CHIBIOS
#    if CH_CFG_USE_MEMCORE == FALSE
#        error ChibiOS is configured without a memory allocator. Your keyboard may have set `#define CH_CFG_USE_MEMCORE FALSE`, which is incompatible with this debounce algorithm.
#    endif
#endif

#ifndef DEBOUNCE
#    define DEBOUNCE 5
#endif

// Maximum debounce: 255ms
#if DEBOUNCE > UINT8_MAX
#    undef DEBOUNCE
#    define DEBOUNCE UINT8_MAX
#endif

typedef uint8_t debounce_counter_t;

#if DEBOUNCE > 0

#    define DEBOUNCE_ELAPSED 0

static debounce_counter_t *debounce_counters;

static matrix_row_t *last_raw;
static fast_timer_t  last_time;
static bool          counters_need_update;
static bool          cooked_changed;

static void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, uint8_t elapsed_time);
static void start_debounce_counters(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows);

// we use num_rows rather than MATRIX_ROWS to support split keyboards
void debounce_init(uint8_t num_rows) {
    debounce_counters = (debounce_counter_t *)malloc(num_rows * sizeof(debounce_counter_t));
    last_raw          = (matrix_row_t *)calloc(num_rows, sizeof(debounce_counter_t));

    debounce_counter_t *debounce_pointer = debounce_counters;
    for (uint8_t row = 0; row < num_rows; row++, debounce_pointer++) {
        *debounce_pointer = DEBOUNCE_ELAPSED;
    }
}

void debounce_free(void) {
    free(debounce_counters);
    debounce_counters = NULL;
    free(last_raw);
    last_raw = NULL;
}

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, bool changed) {
    bool updated_last = false;
    cooked_changed    = false;

    if (counters_need_update) {
        fast_timer_t now          = timer_read_fast();
        fast_timer_t elapsed_time = TIMER_DIFF_FAST(now, last_time);

        last_time    = now;
        updated_last = true;
        if (elapsed_time > UINT8_MAX) {
            elapsed_time = UINT8_MAX;
        }

        if (elapsed_time > 0) {
            update_debounce_counters_and_transfer_if_expired(raw, cooked, num_rows, elapsed_time);
        }
    }

    if (changed) {
        if (!updated_last) {
            last_time = timer_read_fast();
        }
        start_debounce_counters(raw, cooked, num_rows);
    }

    return cooked_changed;
}

static void update_debounce_counters_and_transfer_if_expired(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows, uint8_t elapsed_time) {
    counters_need_update = false;

    debounce_counter_t *debounce_pointer = debounce_counters;
    for (uint8_t row = 0; row < num_rows; row++, debounce_pointer++) {
        debounce_counter_t debounce_counter = *debounce_pointer;
        if (debounce_counter != DEBOUNCE_ELAPSED) {
            if (debounce_counter <= elapsed_time) {
                *debounce_pointer    = DEBOUNCE_ELAPSED;
                matrix_row_t raw_row = raw[row];
                if (last_raw[row] == raw_row) {
                    if (cooked[row] != raw_row) {
                        cooked[row]    = raw_row;
                        cooked_changed = true;
                    }
                }
            } else {
                *debounce_pointer    = debounce_counter - elapsed_time;
                counters_need_update = true;
            }
        }
    }
}

static void start_debounce_counters(matrix_row_t raw[], matrix_row_t cooked[], uint8_t num_rows) {
    debounce_counter_t *debounce_pointer = debounce_counters;
    for (uint8_t row = 0; row < num_rows; row++, debounce_pointer++) {
        matrix_row_t raw_row = raw[row];
        if (last_raw[row] != raw_row) {
            *debounce_pointer    = DEBOUNCE;
            last_raw[row]        = raw_row;
            counters_need_update = true;
        } else if (cooked[row] != raw_row) {
            if (*debounce_pointer == DEBOUNCE_ELAPSED) {
                *debounce_pointer    = DEBOUNCE;
                counters_need_update = true;
            }
        } else {
            *debounce_pointer = DEBOUNCE_ELAPSED;
        }
    }
}

#else
#    include "none.c"
#endif
