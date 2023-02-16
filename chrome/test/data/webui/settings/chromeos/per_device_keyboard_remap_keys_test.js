// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeKeyboards, ModifierKey, Router, routes, SettingsPerDeviceKeyboardRemapKeysElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDeviceKeyboardRemapKeys', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardRemapKeysElement}
   */
  let page = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    page = null;
  });

  async function initializeRemapKeyspage() {
    page = document.createElement('settings-per-device-keyboard-remap-keys');

    // Set the current route with keyboardId as search param and notify
    // the observer to update keyboard settings.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[0].id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    assertTrue(page != null);
    return flushTasks();
  }

  /**
   * Verify that the remap subpage is correctly loaded with keyboard data.
   */
  test('keyboard remap subpage loaded', async () => {
    await initializeRemapKeyspage();
    assertTrue(!!page.keyboard);

    // Verify that the dropdown menu for unremapped key is displayed as default.
    const altKeyDrowDown = page.shadowRoot.querySelector('#altKey');
    const altKeyMappedTo = ModifierKey.ALT.toString();
    assertTrue(!!altKeyDrowDown);
    assertEquals(
        altKeyDrowDown.shadowRoot.querySelector('select').value,
        altKeyMappedTo);

    // Verify that the dropdown menu for remapped key is displayed as the
    // the target key in keyboard remapping settings.
    const ctrlKeyDropdown = page.shadowRoot.querySelector('#ctrlKey');
    const ctrlKeyMappedTo =
        fakeKeyboards[0]
            .settings.modifierRemappings.get(ModifierKey.CONTROL)
            .toString();
    assertTrue(!!ctrlKeyDropdown);
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value,
        ctrlKeyMappedTo);

    // await flushTasks();
    // Verify that the label for meta key is displayed as the
    // the target key in keyboard remapping settings.
    const metaKeyLabel = page.shadowRoot.querySelector('#metaKeyLabel');
    assertTrue(!!metaKeyLabel);
    assertEquals(metaKeyLabel.textContent, 'Command');
  });

  /**
   * Verify that the remap subpage is correctly updated when a different
   * keyboardId is passed through the query url.
   */
  test('keyboard remap subpage updated for different keyboard', async () => {
    await initializeRemapKeyspage();

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
    const ctrlKeyDropdown = page.shadowRoot.querySelector('#ctrlKey');
    const ctrlKeyMappedTo = ModifierKey.CONTROL.toString();
    assertTrue(!!ctrlKeyDropdown);
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value,
        ctrlKeyMappedTo);

    // Verify that the dropdown menu for remapped key is updated and displayed
    // as the target key in the new keyboard remapping settings.
    const altKeyDrowDown = page.shadowRoot.querySelector('#altKey');
    const altKeyMappedTo = fakeKeyboards[2]
                               .settings.modifierRemappings.get(ModifierKey.ALT)
                               .toString();
    assertTrue(!!altKeyDrowDown);
    assertEquals(
        altKeyDrowDown.shadowRoot.querySelector('select').value,
        altKeyMappedTo);

    // Verify that the label for meta key is displayed as the
    // the target key in the new keyboard remapping settings.
    const metaKeyLabel = page.shadowRoot.querySelector('#metaKeyLabel');
    assertTrue(!!metaKeyLabel);
    assertEquals(metaKeyLabel.textContent, 'Launcher');
  });

  /**
   * Verify that the restore defaults button will restore the remapping keys.
   */
  test('keyboard remap subpage restore defaults', async () => {
    await initializeRemapKeyspage();

    // Click the restore defaults button.
    const restoreButton =
        page.shadowRoot.querySelector('#restoreDefaultsButton');
    assertTrue(!!restoreButton);
    restoreButton.click();
    await flushTasks();

    // The keyboard has "Command" as metaKey, so ctrl key should be restored to
    // void, meta key should be restored to ctrl.
    const ctrlKeyDropdown = page.shadowRoot.querySelector('#ctrlKey');
    assertTrue(!!ctrlKeyDropdown);
    const metaKeyValue = ModifierKey.META.toString();
    assertEquals(
        ctrlKeyDropdown.shadowRoot.querySelector('select').value, metaKeyValue);

    const ctrlKeyValue = ModifierKey.CONTROL.toString();
    const metaKeyDropdown = page.shadowRoot.querySelector('#metaKey');
    assertEquals(
        metaKeyDropdown.shadowRoot.querySelector('select').value, ctrlKeyValue);

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
    // The keyboard has "Launcher" as metaKey, ctrl key should be restored to
    // default key mappings.
    const altKeyValue = ModifierKey.ALT.toString();
    const altKeyDrowDown = page.shadowRoot.querySelector('#altKey');
    assertTrue(!!altKeyDrowDown);
    assertEquals(
        altKeyDrowDown.shadowRoot.querySelector('select').value, altKeyValue);
    assertEquals(
        metaKeyDropdown.shadowRoot.querySelector('select').value, metaKeyValue);
  });
});