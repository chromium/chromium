// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeTouchpads, SettingsPerDeviceTouchpadSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PerDeviceTouchpadSubsection', function() {
  /**
   * @type {?SettingsPerDeviceTouchpadSubsectionElement}
   */
  let subsection = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    subsection = null;
  });

  /**
   * @return {!Promise}
   */
  function initializePerDeviceTouchpadSubsection() {
    subsection =
        document.createElement('settings-per-device-touchpad-subsection');
    assertTrue(subsection != null);
    subsection.touchpad = fakeTouchpads[0];
    subsection.allowScrollSettings_ = true;
    document.body.appendChild(subsection);
    return flushTasks();
  }

  /**
   * @param {!Boolean} isHaptic
   * @return {!Promise}
   */
  function changeIsHapticState(isHaptic) {
    subsection.touchpad = {...subsection.touchpad, isHaptic: isHaptic};
    return flushTasks();
  }

  /**
   * @param {!Object} touchpad
   * @param {!Boolean} allowScrollSettings
   * @return {!Promise}
   */
  function changeTouchpadSubsectionState(touchpad, allowScrollSettings) {
    subsection.touchpad = touchpad;
    subsection.allowScrollSettings_ = allowScrollSettings;
    return flushTasks();
  }

  /**Test that touchpad settings data are from the touchpad provider.*/
  test('Verify touchpad settings data', async () => {
    await initializePerDeviceTouchpadSubsection();
    let enableTapToClickButton =
        subsection.shadowRoot.querySelector('#enableTapToClick');
    assertEquals(
        fakeTouchpads[0].settings.tapToClickEnabled,
        enableTapToClickButton.pref.value);
    let enableTapDraggingButton =
        subsection.shadowRoot.querySelector('#enableTapDragging');
    assertEquals(
        fakeTouchpads[0].settings.tapDraggingEnabled,
        enableTapDraggingButton.pref.value);
    let touchpadAcceleration =
        subsection.shadowRoot.querySelector('#touchpadAcceleration');
    assertEquals(
        fakeTouchpads[0].settings.accelerationEnabled,
        touchpadAcceleration.pref.value);
    let touchpadScrollAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadScrollAcceleration');
    assertTrue(isVisible(touchpadScrollAccelerationButton));
    assertEquals(
        fakeTouchpads[0].settings.scrollAcceleration,
        touchpadScrollAccelerationButton.pref.value);
    let touchpadScrollSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#touchpadScrollSpeedSlider'));
    assertTrue(isVisible(touchpadScrollSpeedSlider));
    assertEquals(
        fakeTouchpads[0].settings.hapticSensitivity,
        touchpadScrollSpeedSlider.pref.value);
    let touchpadSensitivitySlider =
        assert(subsection.shadowRoot.querySelector('#touchpadSensitivity'));
    assertEquals(
        fakeTouchpads[0].settings.sensitivity,
        touchpadSensitivitySlider.pref.value);
    let touchpadHapticClickSensitivitySlider = assert(
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity'));
    assertTrue(isVisible(touchpadHapticClickSensitivitySlider));
    assertEquals(
        fakeTouchpads[0].settings.scrollSensitivity,
        touchpadHapticClickSensitivitySlider.pref.value);
    let touchpadHapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertTrue(isVisible(touchpadHapticFeedbackToggleButton));
    assertEquals(
        fakeTouchpads[0].settings.hapticEnabled,
        touchpadHapticFeedbackToggleButton.checked);
    assertEquals(
        fakeTouchpads[0].settings.reverseScrolling,
        subsection.reverseScrollValue);

    await changeTouchpadSubsectionState(fakeTouchpads[1], false);
    enableTapToClickButton =
        subsection.shadowRoot.querySelector('#enableTapToClick');
    assertEquals(
        fakeTouchpads[1].settings.tapToClickEnabled,
        enableTapToClickButton.pref.value);
    enableTapDraggingButton =
        subsection.shadowRoot.querySelector('#enableTapDragging');
    assertEquals(
        fakeTouchpads[1].settings.tapDraggingEnabled,
        enableTapDraggingButton.pref.value);
    touchpadAcceleration =
        subsection.shadowRoot.querySelector('#touchpadAcceleration');
    assertEquals(
        fakeTouchpads[1].settings.accelerationEnabled,
        touchpadAcceleration.pref.value);
    touchpadScrollAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadScrollAcceleration');
    assertFalse(isVisible(touchpadScrollAccelerationButton));
    touchpadScrollSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#touchpadScrollSpeedSlider'));
    assertFalse(isVisible(touchpadScrollSpeedSlider));
    touchpadSensitivitySlider =
        assert(subsection.shadowRoot.querySelector('#touchpadSensitivity'));
    assertEquals(
        fakeTouchpads[1].settings.sensitivity,
        touchpadSensitivitySlider.pref.value);
    touchpadHapticClickSensitivitySlider = assert(
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity'));
    assertFalse(isVisible(touchpadHapticClickSensitivitySlider));
    touchpadHapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertFalse(isVisible(touchpadHapticFeedbackToggleButton));
    assertEquals(
        fakeTouchpads[1].settings.reverseScrolling,
        subsection.reverseScrollValue);
  });

  /**
   * Test haptic settings are correctly show or hidden based on the touchpad is
   * haptic or not.
   */
  test('Verify haptic settings visbility', async () => {
    await initializePerDeviceTouchpadSubsection();
    // Change the isHaptic state to true.
    await changeIsHapticState(true);
    // Verify haptic click sensitivity slider is visible in the page.
    let hapticClickSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity');
    assertTrue(isVisible(hapticClickSensitivitySlider));

    // Verify haptic feedback toggle button is visible in the page.
    let hapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertTrue(isVisible(hapticFeedbackToggleButton));

    // Change the isHaptic state to false.
    await changeIsHapticState(false);
    // Verify haptic click sensitivity slider is not visible in the page.
    hapticClickSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity');
    assertFalse(isVisible(hapticClickSensitivitySlider));

    // Verify haptic feedback toggle button is not visible in the page.
    hapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertFalse(isVisible(hapticFeedbackToggleButton));
  });
});