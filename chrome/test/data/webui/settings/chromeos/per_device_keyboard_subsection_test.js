// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeKeyboards, SettingsPerDeviceKeyboardSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

  /**
   * @return {!Promise}
   */
  function initializePerDeviceKeyboardSubsection() {
    subsection =
        document.createElement('settings-per-device-keyboard-subsection');
    assertTrue(subsection != null);
    subsection.keyboard = fakeKeyboards[0];
    document.body.appendChild(subsection);
    return flushTasks();
  }

  /**
   * @param {!Boolean} isExternal
   * @return {!Promise}
   */
  function changeIsExternalState(isExternal) {
    subsection.keyboard = {...subsection.keyboard, isExternal: isExternal};
    return flushTasks();
  }

  /**
   * Test that keyboard settings are correctly show or hidden based on internal
   * vs external.
   */
  test('Verify keyboard settings visbility', async () => {
    await initializePerDeviceKeyboardSubsection();
    // Change the isExternal state to true.
    await changeIsExternalState(true);
    // Verify external top-row are function keys toggle button is visible in the
    // page.
    let externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(externalTopRowAreFunctionKeysButton));

    // Verify block meta function key rewrites toggle button is visible in the
    // page.
    let blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    assertTrue(isVisible(blockMetaFunctionKeyRewritesButton));

    // Verify internal top-row are function keys toggle button is not visible in
    // the page.
    let internalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#internalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(internalTopRowAreFunctionKeysButton));

    // Change the isExternal state to true.
    await changeIsExternalState(false);
    // Verify external top-row are function keys toggle button is not visible in
    // the page.
    externalTopRowAreFunctionKeysButton = subsection.shadowRoot.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(externalTopRowAreFunctionKeysButton));

    // Verify block meta function key rewrites toggle button is not visible in
    // the page.
    blockMetaFunctionKeyRewritesButton = subsection.shadowRoot.querySelector(
        '#blockMetaFunctionKeyRewritesButton');
    assertFalse(isVisible(blockMetaFunctionKeyRewritesButton));

    // Verify internal top-row are function keys toggle button is visible in the
    // page.
    internalTopRowAreFunctionKeysButton = subsection.shadowRoot.querySelector(
        '#internalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(internalTopRowAreFunctionKeysButton));
  });

  /**
   * Test that expected html elements are in the page after loaded.
   */
  test('per-device keyboard subsection loaded', async () => {
    await initializePerDeviceKeyboardSubsection();
    // Verify the external top-row are function keys toggle button is in the
    // page.
    const externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assertTrue(!!externalTopRowAreFunctionKeysButton);

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