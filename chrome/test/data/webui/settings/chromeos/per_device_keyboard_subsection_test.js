// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {FakeInputDeviceSettingsProvider, fakeKeyboards, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PerDeviceKeyboardSubsection', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardSubsectionElement}
   */
  let subsection = null;
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    subsection = null;
    provider = null;
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * @return {!Promise}
   */
  function initializePerDeviceKeyboardSubsection() {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    setInputDeviceSettingsProviderForTesting(provider);
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
   * @param {!Object} keyboard
   * @return {!Promise}
   */
  function changeKeyboardState(keyboard) {
    subsection.keyboard = keyboard;
    return flushTasks();
  }

  /**Test that API are updated when keyboard settings change.*/
  test('Update API when keyboard settings change', async () => {
    await initializePerDeviceKeyboardSubsection();

    const externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    externalTopRowAreFunctionKeysButton.click();
    await flushTasks();
    let updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.topRowAreFKeys,
        externalTopRowAreFunctionKeysButton.pref.value);

    const blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    blockMetaFunctionKeyRewritesButton.click();
    await flushTasks();
    updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.suppressMetaFKeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref.value);

    const enableAutoRepeatButton =
        subsection.shadowRoot.querySelector('#enableAutoRepeatButton');
    enableAutoRepeatButton.click();
    await flushTasks();
    updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.autoRepeatEnabled,
        enableAutoRepeatButton.pref.value);

    const delaySlider =
        assert(subsection.shadowRoot.querySelector('#delaySlider'));
    MockInteractions.pressAndReleaseKeyOn(
        delaySlider.shadowRoot.querySelector('cr-slider'), 39 /* right */, [],
        'ArrowRight');
    await flushTasks();
    updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.autoRepeatDelay, delaySlider.pref.value);

    const repeatRateSlider =
        assert(subsection.shadowRoot.querySelector('#repeatRateSlider'));
    MockInteractions.pressAndReleaseKeyOn(
        repeatRateSlider.shadowRoot.querySelector('cr-slider'), 39 /* right */,
        [], 'ArrowRight');
    await flushTasks();
    updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.autoRepeatInterval,
        repeatRateSlider.pref.value);
  });

  /**Test that keyboard settings data are from the keyboard provider.*/
  test('Verify keyboard settings data', async () => {
    await initializePerDeviceKeyboardSubsection();
    let externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(externalTopRowAreFunctionKeysButton));
    assertEquals(
        fakeKeyboards[0].settings.topRowAreFKeys,
        externalTopRowAreFunctionKeysButton.pref.value);
    let blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    assertTrue(isVisible(blockMetaFunctionKeyRewritesButton));
    assertEquals(
        fakeKeyboards[0].settings.suppressMetaFKeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref.value);
    let internalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#internalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(internalTopRowAreFunctionKeysButton));
    let enableAutoRepeatButton =
        subsection.shadowRoot.querySelector('#enableAutoRepeatButton');
    assertEquals(
        fakeKeyboards[0].settings.autoRepeatEnabled,
        enableAutoRepeatButton.pref.value);
    let delaySlider =
        assert(subsection.shadowRoot.querySelector('#delaySlider'));
    assertEquals(
        fakeKeyboards[0].settings.autoRepeatDelay, delaySlider.pref.value);
    let repeatRateSlider =
        assert(subsection.shadowRoot.querySelector('#repeatRateSlider'));
    assertEquals(
        fakeKeyboards[0].settings.autoRepeatInterval,
        repeatRateSlider.pref.value);

    changeKeyboardState(fakeKeyboards[1]);
    externalTopRowAreFunctionKeysButton = subsection.shadowRoot.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(externalTopRowAreFunctionKeysButton));
    blockMetaFunctionKeyRewritesButton = subsection.shadowRoot.querySelector(
        '#blockMetaFunctionKeyRewritesButton');
    assertFalse(isVisible(blockMetaFunctionKeyRewritesButton));
    internalTopRowAreFunctionKeysButton = subsection.shadowRoot.querySelector(
        '#internalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(internalTopRowAreFunctionKeysButton));
    assertEquals(
        fakeKeyboards[1].settings.topRowAreFKeys,
        internalTopRowAreFunctionKeysButton.pref.value);
    enableAutoRepeatButton =
        subsection.shadowRoot.querySelector('#enableAutoRepeatButton');
    assertEquals(
        fakeKeyboards[1].settings.autoRepeatEnabled,
        enableAutoRepeatButton.pref.value);
    delaySlider = assert(subsection.shadowRoot.querySelector('#delaySlider'));
    assertEquals(
        fakeKeyboards[1].settings.autoRepeatDelay, delaySlider.pref.value);
    repeatRateSlider =
        assert(subsection.shadowRoot.querySelector('#repeatRateSlider'));
    assertEquals(
        fakeKeyboards[1].settings.autoRepeatInterval,
        repeatRateSlider.pref.value);
  });

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

    // Change the isExternal state to false.
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
  });

  /**
   * Verify the Keyboard remap keys row label is loaded, and sub-label is
   * correctly displayed when the keyboard has 2, 1 or 0 remapped keys.
   */
  test('remap keys sub-label displayed correctly', async () => {
    await initializePerDeviceKeyboardSubsection();
    const remapKeysRow =
        subsection.shadowRoot.querySelector('#remapKeyboardKeys');
    assertTrue(!!remapKeysRow);
    assertEquals(
        'Remap keyboard keys',
        remapKeysRow.shadowRoot.querySelector('#label').textContent.trim());

    const remapKeysSubLabel =
        remapKeysRow.shadowRoot.querySelector('#subLabel');
    assertTrue(!!remapKeysSubLabel);
    assertEquals(2, subsection.keyboard.settings.modifierRemappings.size);
    assertEquals('2 remapped keys', remapKeysSubLabel.textContent.trim());

    await changeKeyboardState(fakeKeyboards[2]);
    assertEquals(1, subsection.keyboard.settings.modifierRemappings.size);
    assertEquals('1 remapped key', remapKeysSubLabel.textContent.trim());

    await changeKeyboardState(fakeKeyboards[1]);
    assertEquals(0, subsection.keyboard.settings.modifierRemappings.size);
    assertEquals('', remapKeysSubLabel.textContent.trim());
  });

  /**
   * Verify clicking the Keyboard remap keys button will be redirecting to the
   * remapped keys subpage.
   */
  test('click remap keys button redirect to new subpage', async () => {
    await initializePerDeviceKeyboardSubsection();
    const remapKeysRow =
        subsection.shadowRoot.querySelector('#remapKeyboardKeys');
    assertTrue(!!remapKeysRow);
    remapKeysRow.click();

    await flushTasks();
    assertEquals(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        Router.getInstance().currentRoute);

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('keyboardId');
    assertTrue(!!urlSearchQuery);
    assertFalse(isNaN(urlSearchQuery));
    const keyboardId = Number(urlSearchQuery);
    const expectedKeyboardId = subsection.keyboard.id;
    assertEquals(expectedKeyboardId, keyboardId);
  });
});
