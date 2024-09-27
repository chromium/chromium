// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FaceGazeCursorCardElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

const DEFAULT_CURSOR_SMOOTHING = 7;
const DEFAULT_CURSOR_SPEED = 10;
const DEFAULT_VELOCITY_THRESHOLD = 9;

const CURSOR_SPEED_STEP = 5;

suite('<facegaze-cursor-card>', () => {
  let faceGazeCursorCard: FaceGazeCursorCardElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeCursorCard = document.createElement('facegaze-cursor-card');
    faceGazeCursorCard.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeCursorCard);
    flush();
  }

  async function pressArrowOnSlider(
      sliderElement: SettingsSliderElement, isRight: boolean) {
    const slider = sliderElement.shadowRoot!.querySelector('cr-slider');
    assert(slider);
    if (isRight) {
      pressAndReleaseKeyOn(
          /*target=*/ slider, /*keyCode=*/ 39, /*modifiers=*/[],
          /*key=*/ 'ArrowRight');
    } else {
      pressAndReleaseKeyOn(
          /*target=*/ slider, /*keyCode=*/ 37, /*modifiers=*/[],
          /*key=*/ 'ArrowLeft');
    }
    await flushTasks();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    faceGazeCursorCard.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('cursor control enabled button syncs to pref', async () => {
    await initPage();

    const prefs = faceGazeCursorCard.prefs.settings.a11y.face_gaze;

    assertTrue(prefs.cursor_control_enabled.value);

    const button = faceGazeCursorCard.shadowRoot!
                       .querySelector<SettingsToggleButtonElement>(
                           '#faceGazeCursorControlEnabledButton');
    assert(button);
    assertTrue(isVisible(button));
    assertTrue(button.checked);

    button.click();
    flush();

    assertFalse(button.checked);
    assertFalse(prefs.cursor_control_enabled.value);
  });

  test(
      'adjust cursor speed separately toggle shows and hides sliders',
      async () => {
        await initPage();

        const prefs = faceGazeCursorCard.prefs.settings.a11y.face_gaze;

        assertFalse(prefs.adjust_speed_separately.value);

        const adjustSpeedsSeparatelyButton =
            faceGazeCursorCard.shadowRoot!
                .querySelector<SettingsToggleButtonElement>(
                    '#faceGazeCursorAdjustSeparatelyButton');
        assert(adjustSpeedsSeparatelyButton);
        assertTrue(isVisible(adjustSpeedsSeparatelyButton));
        assertFalse(adjustSpeedsSeparatelyButton.checked);

        const combinedSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#combinedSpeedSlider');
        assert(combinedSlider);
        assertTrue(isVisible(combinedSlider));
        // Has default value.
        assertEquals(combinedSlider.pref.value, DEFAULT_CURSOR_SPEED);

        // Speed adjustments also have default values.
        assertEquals(prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);

        // Other sliders are hidden.
        let speedUpSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedUpSlider');
        assertNull(speedUpSlider);
        let speedDownSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedDownSlider');
        assertNull(speedDownSlider);
        let speedLeftSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedLeftSlider');
        assertNull(speedLeftSlider);
        let speedRightSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedRightSlider');
        assertNull(speedRightSlider);

        // Clicking the button hides the combined slider and shows the others.
        adjustSpeedsSeparatelyButton.click();
        flush();

        assertTrue(adjustSpeedsSeparatelyButton.checked);
        assertTrue(prefs.adjust_speed_separately.value);

        // Now the combined slider is hidden.
        assertFalse(isVisible(combinedSlider));

        // The individual sliders are all shown and have the default value.
        speedUpSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedUpSlider');
        assert(speedUpSlider);
        assertTrue(isVisible(speedUpSlider));
        assertEquals(speedUpSlider.pref.value, DEFAULT_CURSOR_SPEED);

        speedDownSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedDownSlider');
        assert(speedDownSlider);
        assertTrue(isVisible(speedDownSlider));
        assertEquals(speedDownSlider.pref.value, DEFAULT_CURSOR_SPEED);

        speedLeftSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedLeftSlider');
        assert(speedLeftSlider);
        assertTrue(isVisible(speedLeftSlider));
        assertEquals(speedLeftSlider.pref.value, DEFAULT_CURSOR_SPEED);

        speedRightSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedRightSlider');
        assert(speedRightSlider);
        assertTrue(isVisible(speedRightSlider));
        assertEquals(speedRightSlider.pref.value, DEFAULT_CURSOR_SPEED);
      });

  test('adjusting combined cursor speed adjusts all directions', async () => {
    await initPage();

    const prefs = faceGazeCursorCard.prefs.settings.a11y.face_gaze;

    const combinedSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#combinedSpeedSlider');
    assert(combinedSlider);
    assertTrue(isVisible(combinedSlider));
    assertEquals(combinedSlider.pref.value, DEFAULT_CURSOR_SPEED);

    // Speed prefs have default value.
    assertEquals(prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);

    let value = DEFAULT_CURSOR_SPEED;
    // Adjust the value a few times, all the individual prefs get adjusted.
    for (let i = 0; i < 3; i++) {
      await pressArrowOnSlider(combinedSlider, /*isRight=*/ true);

      value += CURSOR_SPEED_STEP;
      assertEquals(value, combinedSlider.pref.value);
      assertEquals(prefs.cursor_speed_up.value, value);
      assertEquals(prefs.cursor_speed_down.value, value);
      assertEquals(prefs.cursor_speed_left.value, value);
      assertEquals(prefs.cursor_speed_right.value, value);
    }

    // Showing the individual sliders shows they've taken on the value of
    // the combined slider.
    const adjustSpeedsSeparatelyButton =
        faceGazeCursorCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#faceGazeCursorAdjustSeparatelyButton');
    assert(adjustSpeedsSeparatelyButton);
    adjustSpeedsSeparatelyButton.click();
    flush();

    // The individual sliders are all shown and have the updated value.
    const speedUpSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedUpSlider');
    assert(speedUpSlider);
    assertTrue(isVisible(speedUpSlider));
    assertEquals(speedUpSlider.pref.value, value);

    const speedDownSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedDownSlider');
    assert(speedDownSlider);
    assertTrue(isVisible(speedDownSlider));
    assertEquals(speedDownSlider.pref.value, value);

    const speedLeftSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedLeftSlider');
    assert(speedLeftSlider);
    assertTrue(isVisible(speedLeftSlider));
    assertEquals(speedLeftSlider.pref.value, value);

    const speedRightSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedRightSlider');
    assert(speedRightSlider);
    assertTrue(isVisible(speedRightSlider));
    assertEquals(speedRightSlider.pref.value, value);
  });

  test(
      'adjusting cursor speeds separately allows independent adjustments',
      async () => {
        await initPage();

        const prefs = faceGazeCursorCard.prefs.settings.a11y.face_gaze;

        const adjustSpeedsSeparatelyButton =
            faceGazeCursorCard.shadowRoot!
                .querySelector<SettingsToggleButtonElement>(
                    '#faceGazeCursorAdjustSeparatelyButton');
        assert(adjustSpeedsSeparatelyButton);
        adjustSpeedsSeparatelyButton.click();
        flush();

        assertEquals(prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED);
        const speedUpSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedUpSlider');
        assert(speedUpSlider);
        assertTrue(isVisible(speedUpSlider));
        assertEquals(speedUpSlider.pref.value, DEFAULT_CURSOR_SPEED);
        await pressArrowOnSlider(speedUpSlider, /*isRight=*/ true);
        assertEquals(
            speedUpSlider.pref.value, DEFAULT_CURSOR_SPEED + CURSOR_SPEED_STEP);
        assertEquals(
            prefs.cursor_speed_up.value,
            DEFAULT_CURSOR_SPEED + CURSOR_SPEED_STEP);

        assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
        const speedDownSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedDownSlider');
        assert(speedDownSlider);
        assertTrue(isVisible(speedDownSlider));
        assertEquals(speedDownSlider.pref.value, DEFAULT_CURSOR_SPEED);
        await pressArrowOnSlider(speedDownSlider, /*isRight=*/ true);
        await pressArrowOnSlider(speedDownSlider, /*isRight=*/ true);
        assertEquals(
            speedDownSlider.pref.value,
            DEFAULT_CURSOR_SPEED + (CURSOR_SPEED_STEP * 2));
        assertEquals(
            prefs.cursor_speed_down.value,
            DEFAULT_CURSOR_SPEED + (CURSOR_SPEED_STEP * 2));

        assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
        const speedLeftSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedLeftSlider');
        assert(speedLeftSlider);
        assertTrue(isVisible(speedLeftSlider));
        assertEquals(speedLeftSlider.pref.value, DEFAULT_CURSOR_SPEED);
        await pressArrowOnSlider(speedLeftSlider, /*isRight=*/ false);
        assertEquals(
            speedLeftSlider.pref.value,
            DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);
        assertEquals(
            prefs.cursor_speed_left.value,
            DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);

        assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);
        const speedRightSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedRightSlider');
        assert(speedRightSlider);
        assertTrue(isVisible(speedRightSlider));
        assertEquals(speedRightSlider.pref.value, DEFAULT_CURSOR_SPEED);
        await pressArrowOnSlider(speedRightSlider, /*isRight=*/ false);
        assertEquals(
            speedRightSlider.pref.value,
            DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);
        assertEquals(
            prefs.cursor_speed_right.value,
            DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);

        // Turning off "adjust separately" resets to defaults.
        adjustSpeedsSeparatelyButton.click();
        flush();

        const combinedSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#combinedSpeedSlider');
        assert(combinedSlider);
        assertTrue(isVisible(combinedSlider));
        assertEquals(combinedSlider.pref.value, DEFAULT_CURSOR_SPEED);

        assertEquals(prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
        assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);
      });

  test('reset button resets to defaults', async () => {
    await initPage();

    const prefs = faceGazeCursorCard.prefs.settings.a11y.face_gaze;

    // Change the adjust speeds separately value.
    const adjustSpeedsSeparatelyButton =
        faceGazeCursorCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#faceGazeCursorAdjustSeparatelyButton');
    assert(adjustSpeedsSeparatelyButton);
    adjustSpeedsSeparatelyButton.click();
    flush();
    assertTrue(prefs.adjust_speed_separately.value);

    // The individual sliders are all shown, change their values.
    const speedUpSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedUpSlider');
    assert(speedUpSlider);
    assertTrue(isVisible(speedUpSlider));
    assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
    pressArrowOnSlider(speedUpSlider, /*isRight=*/ true);
    flush();
    assertEquals(
        prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED + CURSOR_SPEED_STEP);

    const speedDownSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedDownSlider');
    assert(speedDownSlider);
    assertTrue(isVisible(speedDownSlider));
    assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
    pressArrowOnSlider(speedDownSlider, /*isRight=*/ false);
    flush();
    assertEquals(
        prefs.cursor_speed_down.value,
        DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);

    const speedLeftSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedLeftSlider');
    assert(speedLeftSlider);
    assertTrue(isVisible(speedLeftSlider));
    assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
    pressArrowOnSlider(speedLeftSlider, /*isRight=*/ true);
    flush();
    assertEquals(
        prefs.cursor_speed_left.value,
        DEFAULT_CURSOR_SPEED + CURSOR_SPEED_STEP);

    const speedRightSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#speedRightSlider');
    assert(speedRightSlider);
    assertTrue(isVisible(speedRightSlider));
    assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);
    pressArrowOnSlider(speedRightSlider, /*isRight=*/ false);
    flush();
    assertEquals(
        prefs.cursor_speed_right.value,
        DEFAULT_CURSOR_SPEED - CURSOR_SPEED_STEP);


    const velocityThresholdSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#velocityThresholdSlider');
    assert(velocityThresholdSlider);
    assertTrue(isVisible(velocityThresholdSlider));
    assertEquals(prefs.velocity_threshold.value, DEFAULT_VELOCITY_THRESHOLD);
    pressArrowOnSlider(velocityThresholdSlider, /*isRight=*/ false);
    flush();
    assertEquals(
        prefs.velocity_threshold.value, DEFAULT_VELOCITY_THRESHOLD - 1);

    const cursorSmoothingSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#cursorSmoothingSlider');
    assert(cursorSmoothingSlider);
    assertTrue(isVisible(cursorSmoothingSlider));
    assertEquals(prefs.cursor_smoothing.value, DEFAULT_CURSOR_SMOOTHING);
    pressArrowOnSlider(cursorSmoothingSlider, /*isRight=*/ true);
    flush();
    assertEquals(prefs.cursor_smoothing.value, DEFAULT_CURSOR_SMOOTHING + 1);

    const accelerationButton =
        faceGazeCursorCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#accelerationButton');
    assert(accelerationButton);
    assertTrue(isVisible(accelerationButton));
    accelerationButton.click();
    flush();
    assertFalse(prefs.cursor_use_acceleration.value);

    // Now, reset everything.
    const resetButton =
        faceGazeCursorCard.shadowRoot!.querySelector<CrButtonElement>(
            '#cursorResetButton');
    assert(resetButton);
    assertTrue(isVisible(resetButton));
    resetButton.click();
    flush();

    assertFalse(prefs.adjust_speed_separately.value);
    assertEquals(prefs.velocity_threshold.value, DEFAULT_VELOCITY_THRESHOLD);
    assertEquals(prefs.cursor_smoothing.value, DEFAULT_CURSOR_SMOOTHING);
    assertTrue(prefs.cursor_use_acceleration.value);
    assertEquals(prefs.cursor_speed_up.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_down.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_left.value, DEFAULT_CURSOR_SPEED);
    assertEquals(prefs.cursor_speed_right.value, DEFAULT_CURSOR_SPEED);
  });
});
