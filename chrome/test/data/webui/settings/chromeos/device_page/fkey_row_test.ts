// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {fakeKeyboards, Fkey, FkeyRowElement, Keyboard, Router, routes, SettingsDropdownMenuElement, TopRowActionKey} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

  async function initializeFkeyRow() {
    fkeyRow = document.createElement(FkeyRowElement.is);
    fkeyRow.key = Fkey.F11;
    // Use fakeKeyboards[1] since it represents a Built-in keyboard.
    fkeyRow.keyboard = fakeKeyboards[1] as Keyboard;
    // Set the current route with keyboardId as search param and notify
    // the observer to update keyboard settings.
    const url = new URLSearchParams(
        'keyboardId=' + encodeURIComponent(fakeKeyboards[1]!.id));
    await Router.getInstance().setCurrentRoute(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    document.body.appendChild(fkeyRow);
    return flushTasks();
  }

  function getDropdownMenuElement(): SettingsDropdownMenuElement {
    const dropdown = fkeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(dropdown);
    return dropdown as SettingsDropdownMenuElement;
  }

  test('top row keys mapped to correct label', async () => {
    await initializeFkeyRow();
    const topRowActionKeysTests:
        Array<{topRowActionKeys: TopRowActionKey[], expectedLabel: string}> = [
          {topRowActionKeys: [TopRowActionKey.BACK], expectedLabel: 'back'},
          {
            topRowActionKeys: [TopRowActionKey.REFRESH],
            expectedLabel: 'refresh',
          },
          {
            topRowActionKeys: [TopRowActionKey.OVERVIEW],
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
    await settopRowActionKeys([TopRowActionKey.BACK, TopRowActionKey.REFRESH]);
    assertEquals('back', fkeyRow.getTopRowKeyLabel());
    await setFkey(Fkey.F12);
    assertEquals('refresh', fkeyRow.getTopRowKeyLabel());
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
});
