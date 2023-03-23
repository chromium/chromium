// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {FakeInputDeviceSettingsProvider, fakeKeyboards, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const KEYBOARD_FUNCTION_KEYS_SETTING_ID = 411;

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
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    setInputDeviceSettingsProviderForTesting(provider);
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
    subsection =
        document.createElement('settings-per-device-keyboard-subsection');
    assertTrue(subsection != null);
    const keyboard = fakeKeyboards[0];
    subsection.keyboard = {...keyboard};
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

  async function getConnectedKeyboardSettings() {
    const keyboards = await provider.getConnectedKeyboardSettings();
    return keyboards;
  }

  /**Test that API are updated when keyboard settings change.*/
  test('Update API when keyboard settings change', async () => {
    await initializePerDeviceKeyboardSubsection();

    const externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    externalTopRowAreFunctionKeysButton.click();
    await flushTasks();
    let updatedKeyboards = await getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0].settings.topRowAreFkeys,
        externalTopRowAreFunctionKeysButton.pref.value);

    const blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    blockMetaFunctionKeyRewritesButton.click();
    await flushTasks();

    updatedKeyboards = await getConnectedKeyboardSettings();

    assertEquals(
        updatedKeyboards[0].settings.suppressMetaFkeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref.value);
  });

  /**Test that keyboard settings data are from the keyboard provider.*/
  test('Verify keyboard settings data', async () => {
    await initializePerDeviceKeyboardSubsection();
    let externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(externalTopRowAreFunctionKeysButton));
    assertEquals(
        fakeKeyboards[0].settings.topRowAreFkeys,
        externalTopRowAreFunctionKeysButton.pref.value);
    let blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    assertTrue(isVisible(blockMetaFunctionKeyRewritesButton));
    assertEquals(
        fakeKeyboards[0].settings.suppressMetaFkeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref.value);
    let internalTopRowAreFunctionKeysButton =
        subsection.shadowRoot.querySelector(
            '#internalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(internalTopRowAreFunctionKeysButton));

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
        fakeKeyboards[1].settings.topRowAreFkeys,
        internalTopRowAreFunctionKeysButton.pref.value);
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
    assertEquals(
        2, Object.keys(subsection.keyboard.settings.modifierRemappings).length);
    assertEquals('2 remapped keys', remapKeysSubLabel.textContent.trim());

    await changeKeyboardState(fakeKeyboards[2]);
    assertEquals(
        1, Object.keys(subsection.keyboard.settings.modifierRemappings).length);
    assertEquals('1 remapped key', remapKeysSubLabel.textContent.trim());

    await changeKeyboardState(fakeKeyboards[1]);
    assertEquals(
        0, Object.keys(subsection.keyboard.settings.modifierRemappings).length);
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

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    await initializePerDeviceKeyboardSubsection();
    const topRowAreFunctionKeysToggle = subsection.shadowRoot.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    subsection.keyboardIndex = 0;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=keyboard&settingId=' +
        encodeURIComponent(KEYBOARD_FUNCTION_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    await waitAfterNextRender(topRowAreFunctionKeysToggle);
    assertTrue(!!topRowAreFunctionKeysToggle);
    assertEquals(
        subsection.shadowRoot.activeElement, topRowAreFunctionKeysToggle);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linkng mixin does not focus on second element', async () => {
    await initializePerDeviceKeyboardSubsection();
    const topRowAreFunctionKeysToggle = subsection.shadowRoot.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    subsection.keyboardIndex = 1;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=keyboard&settingId=' +
        encodeURIComponent(KEYBOARD_FUNCTION_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assertTrue(!!topRowAreFunctionKeysToggle);
    assertFalse(!!subsection.shadowRoot.activeElement);
  });
});
