/*
 * MIT License
 *
 * Copyright (c) 2022-2025 Andrew Mike and David Volovskiy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "tally_face.h"
#include "watch.h"

#define TALLY_FACE_MAX 9999
#define TALLY_FACE_MIN -999

static bool _init_val;
static bool _quick_ticks_running;

static const int16_t _tally_default[] = {
    0,

#ifdef TALLY_FACE_PRESETS_MTG
    20,
    40,
#endif /* TALLY_FACE_PRESETS_MTG */

#ifdef TALLY_FACE_PRESETS_YUGIOH
    4000,
    8000,
#endif /* TALLY_FACE_PRESETS_YUGIOH */

};

#define TALLY_FACE_PRESETS_SIZE() (sizeof(_tally_default) / sizeof(int16_t))

void tally_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(tally_state_t));
        memset(*context_ptr, 0, sizeof(tally_state_t));
        tally_state_t *state = (tally_state_t *)*context_ptr;
        state->tally_default_idx = 0;
        state->tally_idx = _tally_default[state->tally_default_idx];
        _init_val = true;
    }
}

void tally_face_activate(void *context) {
    (void) context;
    _quick_ticks_running = false;
}

static void start_quick_cyc(void){
    _quick_ticks_running = true;
    movement_request_tick_frequency(8);
}

static void stop_quick_cyc(void){
    _quick_ticks_running = false;
    movement_request_tick_frequency(1);
}

static void tally_face_increment(tally_state_t *state, bool sound_on) {
        bool soundOn = !_quick_ticks_running && sound_on;
        _init_val = false;
        if (state->tally_idx >= TALLY_FACE_MAX){
            if (soundOn) watch_buzzer_play_note(BUZZER_NOTE_E7, 30);
        }
        else {
            state->tally_idx++;
            print_tally(state, sound_on);
            if (soundOn) watch_buzzer_play_note(BUZZER_NOTE_E6, 30);
        }
}

static void tally_face_decrement(tally_state_t *state, bool sound_on) {
        bool soundOn = !_quick_ticks_running && sound_on;
        _init_val = false;
        if (state->tally_idx <= TALLY_FACE_MIN){
            if (soundOn) watch_buzzer_play_note(BUZZER_NOTE_C5SHARP_D5FLAT, 30);
        }
        else {
            state->tally_idx--;
            print_tally(state, sound_on);
            if (soundOn) watch_buzzer_play_note(BUZZER_NOTE_C6SHARP_D6FLAT, 30);
        }
}

static bool tally_face_should_move_back(tally_state_t *state) {
    return state->tally_idx == _tally_default[state->tally_default_idx];
}

bool tally_face_loop(movement_event_t event, void *context) {
    tally_state_t *state = (tally_state_t *)context;
    static bool using_led = false;

    if (using_led) {
        if(!HAL_GPIO_BTN_MODE_read() && !HAL_GPIO_BTN_LIGHT_read() && !HAL_GPIO_BTN_ALARM_read())
            using_led = false;
        else {
            if (event.event_type == EVENT_LIGHT_BUTTON_DOWN || event.event_type == EVENT_ALARM_BUTTON_DOWN)
                movement_illuminate_led();
            return true;
        }
    }

    switch (event.event_type) {
        case EVENT_TICK:
            if (_quick_ticks_running) {
                bool light_pressed = HAL_GPIO_BTN_LIGHT_read();
                bool alarm_pressed = HAL_GPIO_BTN_ALARM_read();
                if (light_pressed && alarm_pressed) stop_quick_cyc();
                else if (light_pressed) tally_face_increment(state, movement_button_should_sound());
                else if (alarm_pressed) tally_face_decrement(state, movement_button_should_sound());
                else stop_quick_cyc();
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            tally_face_decrement(state, movement_button_should_sound());
            break;
        case EVENT_ALARM_LONG_PRESS:
            tally_face_decrement(state, movement_button_should_sound());
            start_quick_cyc();
            break;
        case EVENT_MODE_LONG_PRESS:
            if (tally_face_should_move_back(state)) {
                _init_val = true;
                movement_move_to_face(0);
            }
            else {
                state->tally_idx = _tally_default[state->tally_default_idx]; // reset tally index
                _init_val = true;
                //play a reset tune
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_G6, 30);
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_REST, 30);
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_E6, 30);
                print_tally(state, movement_button_should_sound());
            }
            break;
        case EVENT_LIGHT_BUTTON_UP:
            tally_face_increment(state, movement_button_should_sound());
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
        case EVENT_ALARM_BUTTON_DOWN:
            if (HAL_GPIO_BTN_MODE_read()) {
                movement_illuminate_led();
                using_led = true;
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            if (TALLY_FACE_PRESETS_SIZE() > 1 && _init_val){
                state->tally_default_idx = (state->tally_default_idx + 1) % TALLY_FACE_PRESETS_SIZE();
                state->tally_idx = _tally_default[state->tally_default_idx];
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_E6, 30);
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_REST, 30);
                if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_G6, 30);
                print_tally(state, movement_button_should_sound());
            }
            else{
                tally_face_increment(state, movement_button_should_sound());
                start_quick_cyc();
            }
            break;
        case EVENT_ACTIVATE:
            print_tally(state, movement_button_should_sound());
            break;
        case EVENT_TIMEOUT:
            // ignore timeout
            break;
        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

// print tally index at the center of display.
void print_tally(tally_state_t *state, bool sound_on) {
    char buf[6];
    int display_val = (int)(state->tally_idx);
    
    // Clamp to limits
    if (display_val > TALLY_FACE_MAX) display_val = TALLY_FACE_MAX;
    if (display_val < TALLY_FACE_MIN) display_val = TALLY_FACE_MIN;
    
    if (sound_on)
        watch_set_indicator(WATCH_INDICATOR_BELL);
    else
        watch_clear_indicator(WATCH_INDICATOR_BELL);
    watch_display_text_with_fallback(WATCH_POSITION_TOP, "TALLY", "TA");
    sprintf(buf, "%4d", display_val);
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

void tally_face_resign(void *context) {
    (void) context;
}
