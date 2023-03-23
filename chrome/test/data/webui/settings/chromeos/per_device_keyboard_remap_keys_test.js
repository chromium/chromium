// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakeKeyboards, KeyboardRemapModifierKeyRowElement, MetaKey, ModifierKey, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardRemapKeysElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDeviceKeyboardRemapKeys', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardRemapKeysElement}
   */
  let page = null;

  /**
   *  @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;
  setup(() => {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    setInputDeviceSettingsProviderForTesting(provider);
    PolymerTest.clearBody();
  });

  teardown(() => {
    page = null;
    provider = null;
  });

  async function initializeRemapKeysPage(keyboards = fakeKeyboards) {
    page = document.createElement('settings-per-device-keyboard-remap-keys');
    page.keyboards = keyboards;
    // Set the current route with keyboardId as search param and notify
    // the observer to update keyboard settings.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(keyboards[0].id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    assertTrue(page != null);
    return flushTasks();
  }

  async function setKeyboards(keyboards) {
    page.keyboards = keyboards;
    return flushTasks();
  }

  /**
   * Check that all the prefs are set to default keyboard value.
   */
  function checkPrefsSetToDefault() {
    const ctrlDefaultMapping = page.keyboard.metaKey === MetaKey.kCommand ?
        ModifierKey.kMeta :
        ModifierKey.kControl;
    const metaDefaultMapping = page.keyboard.metaKey === MetaKey.kCommand ?
        ModifierKey.kControl :
        ModifierKey.kMeta;
    assertEquals(page.fakeAltPref.value, ModifierKey.kAlt);
    assertEquals(page.fakeAssistantPref.value, ModifierKey.kAssistant);
    assertEquals(page.fakeBackspacePref.value, ModifierKey.kBackspace);
    assertEquals(page.fakeCapsLockPref.value, ModifierKey.kCapsLock);
    assertEquals(page.fakeCtrlPref.value, ctrlDefaultMapping);
    assertEquals(page.fakeEscPref.value, ModifierKey.kEscape);
    assertEquals(page.fakeMetaPref.value, metaDefaultMapping);
  }

  /**
   * Verify that the remap subpage is correctly loaded with keyboard data.
   */
  test('keyboard remap subpage loaded', async () => {
    await initializeRemapKeysPage();
    assertTrue(!!page.keyboard);

    // Verify that the dropdown menu for unremapped key is displayed as default.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const altKeyRow = page.shadowRoot.querySelector('#altKey');
    const altKeyDropdown = altKeyRow.shadowRoot.querySelector('#keyDropdown');
    const altKeyMappedTo = ModifierKey.kAlt.toString();
    assertTrue(!!altKeyDropdown);
    assertEquals(
        altKeyDropdown.shadowRoot.querySelector('select').value,
        altKeyMappedTo);

    // Verify that the default key icon is not highlighted.
    assertEquals(altKeyRow.keyState, 'default-remapping');

    // Verify that the dropdown menu for remapped key is displayed as the
    // the target key in keyboard remapping settings.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const ctrlKeyRow = page.shadowRoot.querySelector('#ctrlKey');
    const ctrlKeyMappedTo =
        fakeKeyboards[0]
            .settings.modifierRemappings[ModifierKey.kControl]
            .toString();
    const ctrlKeyDropdown = ctrlKeyRow.shadowRoot.querySelector('#keyDropdown');
    assertTrue(!!ctrlKeyDropdown);
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value,
        ctrlKeyMappedTo);
    // Verify that the remapped key icon is highlighted.
    assertEquals(ctrlKeyRow.keyState, 'modifier-remapped');

    // Verify that the label for meta key is displayed as the
    // the target key in keyboard remapping settings.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const metaKeyRow = page.shadowRoot.querySelector('#metaKey');
    assertTrue(!!metaKeyRow);
    assertEquals(metaKeyRow.keyLabel, 'Command');

    // Verify that the icon is hidden.
    const commandKeyIcon = metaKeyRow.shadowRoot.querySelector('iron-icon');
    assertFalse(!!commandKeyIcon);
  });

  /**
   * Verify that the remap subpage is correctly updated when a different
   * keyboardId is passed through the query url.
   */
  test('keyboard remap subpage updated for different keyboard', async () => {
    await initializeRemapKeysPage();

    // Update the subpage with a new keyboard.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[2].id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    assertTrue(!!page.keyboard);
    await flushTasks();

    // Verify that the dropdown menu for unremapped key in the new
    // keyboard is updated and displayed as default.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const ctrlKeyRow = page.shadowRoot.querySelector('#ctrlKey');
    const ctrlKeyDropdown = ctrlKeyRow.shadowRoot.querySelector('#keyDropdown');
    const ctrlKeyMappedTo = ModifierKey.kControl.toString();
    assertTrue(!!ctrlKeyDropdown);
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value,
        ctrlKeyMappedTo);
    // Verify that the default key icon is not highlighted.
    assertEquals(ctrlKeyRow.keyState, 'default-remapping');

    // Verify that the dropdown menu for remapped key is updated and displayed
    // as the target key in the new keyboard remapping settings.
    const altKeyRow = page.shadowRoot.querySelector('#altKey');
    const altKeyDropDown = altKeyRow.shadowRoot.querySelector('#keyDropdown');
    const altKeyMappedTo = fakeKeyboards[2]
                               .settings.modifierRemappings[ModifierKey.kAlt]
                               .toString();
    assertTrue(!!altKeyDropDown);
    assertEquals(
        altKeyDropDown.shadowRoot.querySelector('select').value,
        altKeyMappedTo);
    // Verify that the remapped key icon is highlighted.
    assertEquals(altKeyRow.keyState, 'modifier-remapped');

    // Verify that the label for meta key is displayed as the
    // the target key in the new keyboard remapping settings.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const metaKeyRow = page.shadowRoot.querySelector('#metaKey');
    assertTrue(!!metaKeyRow);
    assertEquals(metaKeyRow.keyLabel, 'Launcher');

    const launcherKeyIcon = metaKeyRow.shadowRoot.querySelector('iron-icon');
    assertTrue(!!launcherKeyIcon);
    assertEquals('os-settings:launcher', launcherKeyIcon.icon);
  });

  /**
   * Verify that the restore defaults button will restore the remapping keys.
   */
  test('keyboard remap subpage restore defaults', async () => {
    await initializeRemapKeysPage();

    // Click the restore defaults button.
    const restoreButton =
        page.shadowRoot.querySelector('#restoreDefaultsButton');
    assertTrue(!!restoreButton);
    restoreButton.click();
    await flushTasks();

    // The keyboard has "Command" as metaKey, so ctrl key should be restored to
    // meta, meta key should be restored to ctrl.
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const ctrlKeyRow = page.shadowRoot.querySelector('#ctrlKey');
    const ctrlKeyDropdown = ctrlKeyRow.shadowRoot.querySelector('#keyDropdown');
    assertTrue(!!ctrlKeyDropdown);
    const metaKeyValue = ModifierKey.kMeta.toString();
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value, metaKeyValue);
    // Verify that the restored key icon is not highlighted.
    assertEquals(ctrlKeyRow.keyState, 'default-remapping');

    const ctrlKeyValue = ModifierKey.kControl.toString();
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const metaKeyRow = page.shadowRoot.querySelector('#metaKey');
    const metaKeyDropdown = metaKeyRow.shadowRoot.querySelector('#keyDropdown');
    assertEquals(
        metaKeyDropdown.shadowRoot.querySelector('select').value, ctrlKeyValue);
    // Verify that the restored key icon is not highlighted.
    assertEquals(metaKeyRow.keyState, 'default-remapping');

    // Update the subpage with a new keyboard.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[2].id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    assertTrue(!!page.keyboard);
    await flushTasks();

    restoreButton.click();
    await flushTasks();
    // The keyboard has "Launcher" as metaKey, meta key should be restored to
    // default metaKey mappings.
    const altKeyValue = ModifierKey.kAlt.toString();
    /**  @type {?KeyboardRemapModifierKeyRowElement} */
    const altKeyRow = page.shadowRoot.querySelector('#altKey');
    const altKeyDropDown = altKeyRow.shadowRoot.querySelector('#keyDropdown');
    assertTrue(!!altKeyDropDown);
    assertEquals(
        altKeyDropDown.shadowRoot.querySelector('select').value, altKeyValue);
    assertEquals(
        metaKeyDropdown.shadowRoot.querySelector('select').value, metaKeyValue);
    // Verify that the restored key icon is not highlighted.
    assertEquals(metaKeyRow.keyState, 'default-remapping');
  });

  /**
   * Verify that if the keyboard is disconnected while the user is in
   * the remapping page, it will switch back to per device keyboard page.
   */
  test('re-route to back page when keyboard disconnected', async () => {
    await initializeRemapKeysPage();
    // Check it's currently in the modifier remapping page.
    assertEquals(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        Router.getInstance().currentRoute);
    assertEquals(page.keyboardId, page.keyboards[0].id);
    const updatedKeyboards = [fakeKeyboards[1], fakeKeyboards[2]];
    await setKeyboards(updatedKeyboards);
    assertEquals(routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
  });

  /**
   * Test that update keyboard settings api is called when keyboard remapping
   * prefs settings change.
   */
  test('Update keyboard settings', async () => {
    await initializeRemapKeysPage();
    assertTrue(page.isInitialized);
    // Set the modifier remappings to default stage.
    const restoreButton =
        page.shadowRoot.querySelector('#restoreDefaultsButton');
    assertTrue(!!restoreButton);
    restoreButton.click();
    checkPrefsSetToDefault();

    // Change several key remappings in the page.
    page.set('fakeAltPref.value', ModifierKey.kAssistant);
    page.set('fakeBackspacePref.value', ModifierKey.kControl);
    page.set('fakeEscPref.value', ModifierKey.kVoid);

    // Verify that the keyboard settings in the provider are updated.
    const keyboards = page.keyboards;
    assertTrue(!!keyboards);
    const updatedRemapping = keyboards[0].settings.modifierRemappings;
    assertTrue(!!updatedRemapping);
    assertEquals(Object.keys(updatedRemapping).length, 3);
    assertEquals(updatedRemapping[ModifierKey.kAlt], ModifierKey.kAssistant);
    assertEquals(
        updatedRemapping[ModifierKey.kBackspace], ModifierKey.kControl);
    assertEquals(updatedRemapping[ModifierKey.kEscape], ModifierKey.kVoid);
  });
});