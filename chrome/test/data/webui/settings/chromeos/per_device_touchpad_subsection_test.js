// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeTouchpads, SettingsPerDeviceTouchpadSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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