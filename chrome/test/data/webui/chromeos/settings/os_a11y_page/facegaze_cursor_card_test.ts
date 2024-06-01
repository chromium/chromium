// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FaceGazeCursorCardElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

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
    assertTrue(faceGazeCursorCard.prefs.settings.a11y.face_gaze
                   .cursor_control_enabled.value);

    const button = faceGazeCursorCard.shadowRoot!
                       .querySelector<SettingsToggleButtonElement>(
                           '#faceGazeCursorControlEnabledButton');
    assert(button);
    assertTrue(isVisible(button));
    assertTrue(button.checked);

    button.click();
    flush();

    assertFalse(button.checked);
    assertFalse(faceGazeCursorCard.prefs.settings.a11y.face_gaze
                    .cursor_control_enabled.value);
  });

  test(
      'adjust cursor speed separately toggle shows and hides sliders',
      async () => {
        await initPage();

        assertFalse(faceGazeCursorCard.prefs.settings.a11y.face_gaze
                        .adjust_speed_separately.value);

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
        assertEquals(combinedSlider.pref.value, 20);

        // Speed adjustments also have default values.
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
                .value,
            20);

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
        assertTrue(faceGazeCursorCard.prefs.settings.a11y.face_gaze
                       .adjust_speed_separately.value);

        // Now the combined slider is hidden.
        assertFalse(isVisible(combinedSlider));

        // The individual sliders are all shown and have the default value.
        speedUpSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedUpSlider');
        assert(speedUpSlider);
        assertTrue(isVisible(speedUpSlider));
        assertEquals(speedUpSlider.pref.value, 20);

        speedDownSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedDownSlider');
        assert(speedDownSlider);
        assertTrue(isVisible(speedDownSlider));
        assertEquals(speedDownSlider.pref.value, 20);

        speedLeftSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedLeftSlider');
        assert(speedLeftSlider);
        assertTrue(isVisible(speedLeftSlider));
        assertEquals(speedLeftSlider.pref.value, 20);

        speedRightSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedRightSlider');
        assert(speedRightSlider);
        assertTrue(isVisible(speedRightSlider));
        assertEquals(speedRightSlider.pref.value, 20);
      });

  test('adjusting combined cursor speed adjusts all directions', async () => {
    await initPage();

    const combinedSlider =
        faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
            '#combinedSpeedSlider');
    assert(combinedSlider);
    assertTrue(isVisible(combinedSlider));
    // Has default value.
    let value = 20;
    assertEquals(combinedSlider.pref.value, value);

    // Speed prefs have default value.
    assertEquals(
        faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up.value,
        value);
    assertEquals(
        faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
            .value,
        value);
    assertEquals(
        faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
            .value,
        value);
    assertEquals(
        faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
            .value,
        value);

    // Adjust the value a few times, all the individual prefs get adjusted.
    for (let i = 0; i < 3; i++) {
      await pressArrowOnSlider(combinedSlider, /*isRight=*/ true);

      value++;
      assertEquals(value, combinedSlider.pref.value);
      assertEquals(
          faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up
              .value,
          value);
      assertEquals(
          faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
              .value,
          value);
      assertEquals(
          faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
              .value,
          value);
      assertEquals(
          faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
              .value,
          value);
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

        const adjustSpeedsSeparatelyButton =
            faceGazeCursorCard.shadowRoot!
                .querySelector<SettingsToggleButtonElement>(
                    '#faceGazeCursorAdjustSeparatelyButton');
        assert(adjustSpeedsSeparatelyButton);
        adjustSpeedsSeparatelyButton.click();
        flush();

        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up
                .value,
            20);
        const speedUpSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedUpSlider');
        assert(speedUpSlider);
        assertTrue(isVisible(speedUpSlider));
        assertEquals(speedUpSlider.pref.value, 20);
        await pressArrowOnSlider(speedUpSlider, /*isRight=*/ true);
        assertEquals(speedUpSlider.pref.value, 21);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up
                .value,
            21);

        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
                .value,
            20);
        const speedDownSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedDownSlider');
        assert(speedDownSlider);
        assertTrue(isVisible(speedDownSlider));
        assertEquals(speedDownSlider.pref.value, 20);
        await pressArrowOnSlider(speedDownSlider, /*isRight=*/ true);
        await pressArrowOnSlider(speedDownSlider, /*isRight=*/ true);
        assertEquals(speedDownSlider.pref.value, 22);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
                .value,
            22);

        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
                .value,
            20);
        const speedLeftSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedLeftSlider');
        assert(speedLeftSlider);
        assertTrue(isVisible(speedLeftSlider));
        assertEquals(speedLeftSlider.pref.value, 20);
        await pressArrowOnSlider(speedLeftSlider, /*isRight=*/ false);
        assertEquals(speedLeftSlider.pref.value, 19);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
                .value,
            19);

        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
                .value,
            20);
        const speedRightSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#speedRightSlider');
        assert(speedRightSlider);
        assertTrue(isVisible(speedRightSlider));
        assertEquals(speedRightSlider.pref.value, 20);
        await pressArrowOnSlider(speedRightSlider, /*isRight=*/ false);
        await pressArrowOnSlider(speedRightSlider, /*isRight=*/ false);
        assertEquals(speedRightSlider.pref.value, 18);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
                .value,
            18);

        // Turning off "adjust separately" resets to defaults.
        adjustSpeedsSeparatelyButton.click();
        flush();

        const combinedSlider =
            faceGazeCursorCard.shadowRoot!.querySelector<SettingsSliderElement>(
                '#combinedSpeedSlider');
        assert(combinedSlider);
        assertTrue(isVisible(combinedSlider));
        assertEquals(combinedSlider.pref.value, 20);

        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_up
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_down
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_left
                .value,
            20);
        assertEquals(
            faceGazeCursorCard.prefs.settings.a11y.face_gaze.cursor_speed_right
                .value,
            20);
      });
});
