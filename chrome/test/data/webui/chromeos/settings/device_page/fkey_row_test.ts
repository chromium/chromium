// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {fakeKeyboards, Fkey, FkeyRowElement, Keyboard, Router, routes, SettingsDropdownMenuElement, TopRowActionKey} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const builtInKeyboard = fakeKeyboards[1];

suite('<fkey-row>', () => {
  let fkeyRow: FkeyRowElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    if (!fkeyRow) {
      return;
    }
    fkeyRow.remove();
  });

  function setFkey(key: Fkey) {
    fkeyRow.key = key;
    return flushTasks();
  }

  function settopRowActionKeys(topRowActionKeys: TopRowActionKey[]) {
    fkeyRow.set('keyboard', {
      ...fkeyRow.get('keyboard'),
      topRowActionKeys,
    });
    return flushTasks();
  }

  async function initializeFkeyRow(keyboard = builtInKeyboard) {
    fkeyRow = document.createElement(FkeyRowElement.is);
    fkeyRow.key = Fkey.F11;
    fkeyRow.keyboard = keyboard as Keyboard;
    // Set the current route with keyboardId as search param and notify
    // the observer to update keyboard settings.
    Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS, new URLSearchParams(), false);
    document.body.appendChild(fkeyRow);
    return flushTasks();
  }

  function getDropdownMenuElement(): SettingsDropdownMenuElement {
    const dropdown = fkeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(dropdown);
    return dropdown as SettingsDropdownMenuElement;
  }

  function getShortcutNames(): string[] {
    // menuOptions.splice(1) is used to remove the "Disabled"
    // dropdown option.
    return getDropdownMenuElement().menuOptions.splice(1).map(m => m.name);
  }

  function shortcutIncludesSearchKey(name: string): boolean {
    return name.includes('search') || name.includes('launcher');
  }

  test('top row keys mapped to correct label', async () => {
    await initializeFkeyRow();
    const topRowActionKeysTests:
        Array<{topRowActionKeys: TopRowActionKey[], expectedLabel: string}> = [
          {topRowActionKeys: [TopRowActionKey.kBack], expectedLabel: 'back'},
          {
            topRowActionKeys: [TopRowActionKey.kRefresh],
            expectedLabel: 'refresh',
          },
          {
            topRowActionKeys: [TopRowActionKey.kOverview],
            expectedLabel: 'overview',
          },
        ];
    for (const {topRowActionKeys, expectedLabel} of topRowActionKeysTests) {
      await settopRowActionKeys(topRowActionKeys);
      assertEquals(expectedLabel, fkeyRow.getTopRowKeyLabel());
    }
  });

  test('Correct top row key (f1/f2) label displayed for fkey', async () => {
    await initializeFkeyRow();
    assertTrue(!!fkeyRow.shadowRoot!.querySelector('#fkeyRow'));
    await settopRowActionKeys(
        [TopRowActionKey.kBack, TopRowActionKey.kRefresh]);
    assertEquals('back', fkeyRow.getTopRowKeyLabel());
    await setFkey(Fkey.F12);
    assertEquals('refresh', fkeyRow.getTopRowKeyLabel());
  });

  test(
      'fkey shortcut options include correct top row action key label',
      async () => {
        await initializeFkeyRow();
        const keyLabel = fkeyRow.getTopRowKeyLabel();
        assertTrue(getShortcutNames().every(
            (name: string) => name.includes(keyLabel)));
      });

  test('fkey shortcut options respect topRowAreFkeys setting', async () => {
    await initializeFkeyRow();
    assertFalse(getShortcutNames().every(shortcutIncludesSearchKey));
    const keyboard = fkeyRow.get('keyboard');
    const kb = {
      ...keyboard,
      settings: {
        ...keyboard.settings,
        topRowAreFkeys: false,
      },
    };
    await initializeFkeyRow(kb);
    assertTrue(getShortcutNames().every(shortcutIncludesSearchKey));
  });

  test(
      'fkey shortcut options include correct top row action key label',
      async () => {
        await initializeFkeyRow();
        const keyLabel = fkeyRow.getTopRowKeyLabel();
        // menuOptions.splice(1) is used to remove the "Disabled"
        // dropdown option.
        const shortcutNames: string[] =
            getDropdownMenuElement().menuOptions.splice(1).map(m => m.name);
        assertTrue(
            shortcutNames.every((name: string) => name.includes(keyLabel)));
      });

  test('fkey shortcut options respect topRowAreFkeys setting', async () => {
    await initializeFkeyRow();
    assertFalse(getShortcutNames().every(shortcutIncludesSearchKey));
    const keyboard = fkeyRow.get('keyboard');
    const kb = {
      ...keyboard,
      settings: {
        ...keyboard.settings,
        topRowAreFkeys: false,
      },
    };
    await initializeFkeyRow(kb);
    assertTrue(getShortcutNames().every(shortcutIncludesSearchKey));
  });
});
