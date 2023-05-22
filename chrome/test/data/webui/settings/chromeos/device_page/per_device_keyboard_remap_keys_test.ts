// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakeKeyboards, Keyboard, KeyboardRemapModifierKeyRowElement, MetaKey, ModifierKey, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardRemapKeysElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-per-device-keyboard-remap-keys>', () => {
  let page: SettingsPerDeviceKeyboardRemapKeysElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    setInputDeviceSettingsProviderForTesting(provider);

    page = document.createElement('settings-per-device-keyboard-remap-keys');
    page.set('keyboards', fakeKeyboards);
    assertFalse(page.get('isInitialized'));
    // Set the current route with keyboardId as search param and notify
    // the observer to update keyboard settings.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[0]!.id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    await flushTasks();
  });

  teardown(() => {
    page.remove();
  });

  function changeKeyboardExternalState(isExternal: boolean): Promise<void> {
    page.keyboard = {...page.keyboard, isExternal};
    return flushTasks();
  }

  function getPageDescription(): string {
    const description =
        page.shadowRoot!.querySelector('#description')!.textContent;
    assert(description);
    return description;
  }

  /**
   * Check that all the prefs are set to default keyboard value.
   */
  function checkPrefsSetToDefault() {
    const ctrlDefaultMapping =
        page.get('keyboard.metaKey') === MetaKey.kCommand ?
        ModifierKey.kMeta :
        ModifierKey.kControl;
    const metaDefaultMapping =
        page.get('keyboard.metaKey') === MetaKey.kCommand ?
        ModifierKey.kControl :
        ModifierKey.kMeta;
    assertEquals(ModifierKey.kAlt, page.get('fakeAltPref.value'));
    assertEquals(ModifierKey.kAssistant, page.get('fakeAssistantPref.value'));
    assertEquals(ModifierKey.kBackspace, page.get('fakeBackspacePref.value'));
    assertEquals(ModifierKey.kCapsLock, page.get('fakeCapsLockPref.value'));
    assertEquals(ctrlDefaultMapping, page.get('fakeCtrlPref.value'));
    assertEquals(ModifierKey.kEscape, page.get('fakeEscPref.value'));
    assertEquals(metaDefaultMapping, page.get('fakeMetaPref.value'));
  }

  /**
   * Verify that the remap subpage is correctly loaded with keyboard data.
   */
  test('keyboard remap subpage loaded', async () => {
    assert(page.get('keyboard'));

    // Verify that the dropdown menu for unremapped key is displayed as default.
    const altKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#altKey');
    assert(altKeyRow);
    assertEquals('alt', altKeyRow.get('keyLabel'));
    const altKeyDropdown = altKeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(altKeyDropdown);
    assertEquals(
        ModifierKey.kAlt.toString(),
        altKeyDropdown.shadowRoot!.querySelector('select')!.value);

    // Verify that the default key icon is not highlighted.
    assertEquals('default-remapping', altKeyRow.keyState);

    // Verify that the dropdown menu for remapped key is displayed as the
    // the target key in keyboard remapping settings.
    const ctrlKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#ctrlKey');
    assert(ctrlKeyRow);
    assertEquals('ctrl', ctrlKeyRow.get('keyLabel'));
    const ctrlKeyMappedTo =
        fakeKeyboards[0]!.settings.modifierRemappings[ModifierKey.kControl]!
            .toString();
    const ctrlKeyDropdown =
        ctrlKeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(ctrlKeyDropdown);
    assertEquals(
        ctrlKeyMappedTo,
        ctrlKeyDropdown.shadowRoot!.querySelector('select')!.value);
    // Verify that the remapped key icon is highlighted.
    assertEquals('modifier-remapped', ctrlKeyRow.keyState);

    // Verify that the label for meta key is displayed as the
    // the target key in keyboard remapping settings.
    const metaKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#metaKey');
    assert(metaKeyRow);
    assertEquals('command', metaKeyRow.get('keyLabel'));

    // Verify that the icon is hidden.
    const commandKeyIcon = metaKeyRow.shadowRoot!.querySelector('iron-icon');
    assertEquals(null, commandKeyIcon);
  });

  /**
   * Verify that the remap subpage is correctly updated when a different
   * keyboardId is passed through the query url.
   */
  test('keyboard remap subpage updated for different keyboard', async () => {
    // Update the subpage with a new keyboard.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[2]!.id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    assert(page.get('keyboard'));
    await flushTasks();

    // Verify that the dropdown menu for unremapped key in the new
    // keyboard is updated and displayed as default.
    const ctrlKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#ctrlKey');
    assert(ctrlKeyRow);
    const ctrlKeyDropdown =
        ctrlKeyRow.shadowRoot!.querySelector('#keyDropdown');
    const ctrlKeyMappedTo = ModifierKey.kControl.toString();
    assert(ctrlKeyDropdown);
    assertEquals(
        ctrlKeyMappedTo,
        ctrlKeyDropdown.shadowRoot!.querySelector('select')!.value);
    // Verify that the default key icon is not highlighted.
    assertEquals('default-remapping', ctrlKeyRow.keyState);

    // Verify that the dropdown menu for remapped key is updated and displayed
    // as the target key in the new keyboard remapping settings.
    const altKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#altKey');
    assert(altKeyRow);
    const altKeyDropDown = altKeyRow.shadowRoot!.querySelector('#keyDropdown');
    const altKeyMappedTo =
        fakeKeyboards[2]!.settings.modifierRemappings[ModifierKey.kAlt]!
            .toString();
    assert(altKeyDropDown);
    assertEquals(
        altKeyMappedTo,
        altKeyDropDown.shadowRoot!.querySelector('select')!.value);
    // Verify that the remapped key icon is highlighted.
    assertEquals('modifier-remapped', altKeyRow.keyState);

    // Verify that the label for meta key is search and the key icon is
    // displayed as launcher.
    const metaKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#metaKey');
    assert(metaKeyRow);
    assertEquals(
        page.i18n('perDeviceKeyboardKeySearch'), metaKeyRow.get('keyLabel'));
    assertEquals('os-settings:launcher', metaKeyRow.get('keyIcon'));

    const launcherKeyIcon = metaKeyRow.shadowRoot!.querySelector('iron-icon');
    assert(launcherKeyIcon);
    assertEquals('os-settings:launcher', launcherKeyIcon.icon);

    // Verify that the label for assistant key is displayed as icon.
    const assistantKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#assistantKey');
    assert(assistantKeyRow);
    assertEquals('assistant', assistantKeyRow.get('keyLabel'));

    const assistantKeyIcon =
        assistantKeyRow.shadowRoot!.querySelector('iron-icon');
    assert(assistantKeyIcon);
    assertEquals('os-settings:assistant', assistantKeyIcon.icon);
  });

  /**
   * Verify that the restore defaults button will restore the remapping keys.
   */
  test('keyboard remap subpage restore defaults', async () => {
    page.restoreDefaults();
    await flushTasks();

    // The keyboard has "Command" as metaKey, so ctrl key should be restored to
    // meta, meta key should be restored to ctrl.
    const ctrlKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#ctrlKey');
    assert(ctrlKeyRow);
    const ctrlKeyDropdown =
        ctrlKeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(ctrlKeyDropdown);
    const metaKeyValue = ModifierKey.kMeta.toString();
    assertEquals(
        metaKeyValue,
        ctrlKeyDropdown.shadowRoot!.querySelector('select')!.value);
    // Verify that the restored key icon is not highlighted.
    assertEquals('default-remapping', ctrlKeyRow.keyState);

    const metaKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#metaKey');
    assert(metaKeyRow);
    const metaKeyDropdown =
        metaKeyRow.shadowRoot!.querySelector('#keyDropdown');
    assertEquals(
        ModifierKey.kControl.toString(),
        metaKeyDropdown!.shadowRoot!.querySelector('select')!.value);
    // Verify that the restored key icon is not highlighted.
    assertEquals('default-remapping', metaKeyRow.keyState);

    // Update the subpage with a new keyboard.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[2]!.id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    assert(page.get('keyboard'));
    await flushTasks();

    page.restoreDefaults();
    await flushTasks();
    // The keyboard has "Launcher" as metaKey, meta key should be restored to
    // default metaKey mappings.
    const altKeyValue = ModifierKey.kAlt.toString();
    const altKeyRow =
        page.shadowRoot!.querySelector<KeyboardRemapModifierKeyRowElement>(
            '#altKey');
    assert(altKeyRow);
    const altKeyDropDown = altKeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(altKeyDropDown);
    assertEquals(
        altKeyValue, altKeyDropDown.shadowRoot!.querySelector('select')!.value);
    assertEquals(
        metaKeyValue,
        metaKeyDropdown!.shadowRoot!.querySelector('select')!.value);
    // Verify that the restored key icon is not highlighted.
    assertEquals('default-remapping', metaKeyRow.keyState);
  });

  /**
   * Verify that if the keyboard is disconnected while the user is in
   * the remapping page, it will switch back to per device keyboard page.
   */
  test('re-route to back page when keyboard disconnected', async () => {
    // Check it's currently in the modifier remapping page.
    assertEquals(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        Router.getInstance().currentRoute);
    assertEquals(page.get('keyboardId'), page.get('keyboards')[0].id);
    const updatedKeyboards = [fakeKeyboards[1], fakeKeyboards[2]] as Keyboard[];
    page.set('keyboards', updatedKeyboards);
    assertEquals(routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);
  });

  /**
   * Test that update keyboard settings api is called when keyboard remapping
   * prefs settings change.
   */
  test('Update keyboard settings', async () => {
    assertTrue(page.get('isInitialized'));
    // Set the modifier remappings to default stage.
    page.restoreDefaults();
    checkPrefsSetToDefault();

    // Change several key remappings in the page.
    page.set('fakeAltPref.value', ModifierKey.kAssistant);
    page.set('fakeBackspacePref.value', ModifierKey.kControl);
    page.set('fakeEscPref.value', ModifierKey.kVoid);

    // Verify that the keyboard settings in the provider are updated.
    const keyboards = await provider.getConnectedKeyboardSettings();
    assert(keyboards);
    const updatedRemapping = keyboards[0]!.settings.modifierRemappings;
    assert(updatedRemapping);
    assertEquals(5, Object.keys(updatedRemapping).length);
    assertEquals(ModifierKey.kAssistant, updatedRemapping[ModifierKey.kAlt]);
    assertEquals(
        ModifierKey.kControl, updatedRemapping[ModifierKey.kBackspace]);
    assertEquals(ModifierKey.kVoid, updatedRemapping[ModifierKey.kEscape]);
    assertEquals(ModifierKey.kControl, updatedRemapping[ModifierKey.kMeta]);
    assertEquals(ModifierKey.kMeta, updatedRemapping[ModifierKey.kControl]);
  });

  test('Keyboard description populated correctly', async () => {
    assertTrue(page.get('isInitialized'));
    assertEquals(
        'For ERGO K860, choose an action for each key', getPageDescription());
    await changeKeyboardExternalState(/* isExternal= */ false);
    assertEquals(
        'For Built-in Keyboard, choose an action for each key',
        getPageDescription());
  });
});
