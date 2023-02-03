// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsPerDeviceKeyboardSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDeviceKeyboardSubsection', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardSubsectionElement}
   */
  let subsection = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    subsection = null;
  });

  function initializePerDeviceKeyboardSubsection() {
    subsection =
        document.createElement('settings-per-device-keyboard-subsection');
    document.body.appendChild(subsection);
    assertTrue(subsection != null);
    document.body.appendChild(subsection);
    return flushTasks();
  }

  test('Initialization Test', async () => {
    await initializePerDeviceKeyboardSubsection();
  });

  /**
   * Test that expected html elements are in the page after loaded.
   */
  test('per-device keyboard subsection loaded', async () => {
    await initializePerDeviceKeyboardSubsection();
    // Verify the top-row are function keys toggle button is in the page.
    const topRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector('#topRowAreFunctionKeysButton');
    assertTrue(!!topRowAreFunctionKeysButton);

    // Verify the enable auto-repeat toggle button is in the page.
    const enableAutoRepeatButton =
        subsection.shadowRoot.querySelector('#enableAutoRepeatButton');
    assertTrue(!!enableAutoRepeatButton);
    enableAutoRepeatButton.click();

    // Verify the repeat delay settings box is in the page.
    const repeatDelayLabel =
        subsection.shadowRoot.querySelector('#repeatDelayLabel');
    assertTrue(!!repeatDelayLabel);
    assertEquals('Delay before repeat', repeatDelayLabel.textContent.trim());
    assertTrue(!!subsection.shadowRoot.querySelector('#delaySlider'));

    // Verify the repeat rate settings box is in the page.
    const repeatRateLabel =
        subsection.shadowRoot.querySelector('#repeatRateLabel');
    assertTrue(!!repeatRateLabel);
    assertEquals('Repeat rate', repeatRateLabel.textContent.trim());
    assertTrue(!!subsection.shadowRoot.querySelector('#repeatRateSlider'));

    // Verify the keyboard remap keys row item is in the page.
    const remapKeysRow =
        subsection.shadowRoot.querySelector('#remapKeyboardKeys');
    assertTrue(!!remapKeysRow);
    assertEquals(
        'Remap keyboard keys',
        remapKeysRow.shadowRoot.querySelector('#label').textContent.trim());
  });
});