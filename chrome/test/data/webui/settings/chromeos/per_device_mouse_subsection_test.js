// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeMice, SettingsPerDeviceMouseSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PerDeviceMouseSubsection', function() {
  /**
   * @type {?SettingsPerDeviceMouseSubsectionElement}
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
  function initializePerDeviceMouseSubsection() {
    subsection = document.createElement('settings-per-device-mouse-subsection');
    assertTrue(subsection != null);
    subsection.mouse = fakeMice[0];
    subsection.allowScrollSettings_ = true;
    document.body.appendChild(subsection);
    return flushTasks();
  }

  /**
   * @param {!Object} mouse
   * @param {!Boolean} allowScrollSettings
   * @return {!Promise}
   */
  function changeMouseSubsectionState(mouse, allowScrollSettings) {
    subsection.mouse = mouse;
    subsection.allowScrollSettings_ = allowScrollSettings;
    return flushTasks();
  }

  /**Test that mouse settings data are from the mouse provider.*/
  test('Verify mouse settings data', async () => {
    await initializePerDeviceMouseSubsection();
    let mouseSwapButtonDropdown =
        subsection.shadowRoot.querySelector('#mouseSwapButtonDropdown');
    assertEquals(
        fakeMice[0].settings.swapRight, mouseSwapButtonDropdown.pref.value);
    let mouseAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#mouseAcceleration');
    assertEquals(
        fakeMice[0].settings.accelerationEnabled,
        mouseAccelerationToggleButton.pref.value);
    let mouseSpeedSlider =
        assert(subsection.shadowRoot.querySelector('#mouseSpeedSlider'));
    assertEquals(fakeMice[0].settings.sensitivity, mouseSpeedSlider.pref.value);
    assertEquals(
        fakeMice[0].settings.reverseScrolling, subsection.reverseScrollValue);
    let mouseScrollAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#mouseScrollAcceleration');
    assertTrue(isVisible(mouseScrollAccelerationToggleButton));
    assertEquals(
        fakeMice[0].settings.scrollAcceleration,
        mouseScrollAccelerationToggleButton.pref.value);
    let mouseScrollSpeedSlider =
        assert(subsection.shadowRoot.querySelector('#mouseScrollSpeedSlider'));
    assertTrue(isVisible(mouseScrollSpeedSlider));
    assertEquals(
        fakeMice[0].settings.scrollSensitivity,
        mouseScrollSpeedSlider.pref.value);

    await changeMouseSubsectionState(fakeMice[1], false);
    mouseSwapButtonDropdown =
        subsection.shadowRoot.querySelector('#mouseSwapButtonDropdown');
    assertEquals(
        fakeMice[1].settings.swapRight, mouseSwapButtonDropdown.pref.value);
    mouseAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#mouseAcceleration');
    assertEquals(
        fakeMice[1].settings.accelerationEnabled,
        mouseAccelerationToggleButton.pref.value);
    mouseSpeedSlider =
        assert(subsection.shadowRoot.querySelector('#mouseSpeedSlider'));
    assertEquals(fakeMice[1].settings.sensitivity, mouseSpeedSlider.pref.value);
    assertEquals(
        fakeMice[1].settings.reverseScrolling, subsection.reverseScrollValue);
    mouseScrollAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#mouseScrollAcceleration');
    assertFalse(isVisible(mouseScrollAccelerationToggleButton));
    mouseScrollSpeedSlider =
        assert(subsection.shadowRoot.querySelector('#mouseScrollSpeedSlider'));
    assertFalse(isVisible(mouseScrollSpeedSlider));
  });
});