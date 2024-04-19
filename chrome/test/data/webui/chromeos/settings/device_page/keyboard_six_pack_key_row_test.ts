// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {KeyboardSixPackKeyRowElement, SettingsDropdownMenuElement, SixPackKey, sixPackKeyProperties} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<keyboard-six-pack-key-row>', () => {
  let sixPackKeyRow: KeyboardSixPackKeyRowElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!sixPackKeyRow) {
      return;
    }
    sixPackKeyRow.remove();
    await flushTasks();
  });

  function initializeKeyboardSixPackKeyRow() {
    sixPackKeyRow = document.createElement(KeyboardSixPackKeyRowElement.is);
    document.body.appendChild(sixPackKeyRow);
    return flushTasks();
  }

  function setKey(key: SixPackKey): Promise<void> {
    sixPackKeyRow.key = key;
    return flushTasks();
  }

  function getMenuOptionsForSixPackKey(key: SixPackKey) {
    return sixPackKeyProperties[key].menuOptions;
  }

  test('Initialize six pack key row', async () => {
    await initializeKeyboardSixPackKeyRow();
    assertTrue(!!sixPackKeyRow.shadowRoot!.querySelector('#sixPackKeyRow'));
  });

  test('Six pack key row displays correct label', async () => {
    await initializeKeyboardSixPackKeyRow();
    const keysToLabelsMap = {
      [SixPackKey.DELETE]: loadTimeData.getString('sixPackKeyLabelDelete'),
      [SixPackKey.HOME]: loadTimeData.getString('sixPackKeyLabelHome'),
      [SixPackKey.END]: loadTimeData.getString('sixPackKeyLabelEnd'),
      [SixPackKey.INSERT]: loadTimeData.getString('sixPackKeyLabelInsert'),
      [SixPackKey.PAGE_DOWN]: loadTimeData.getString('sixPackKeyLabelPageDown'),
      [SixPackKey.PAGE_UP]: loadTimeData.getString('sixPackKeyLabelPageUp'),
    };
    for (const [key, label] of Object.entries(keysToLabelsMap)) {
      await setKey((key as SixPackKey));
      assertEquals(
          label,
          sixPackKeyRow.shadowRoot!.querySelector(
                                       '#keyLabel')!.textContent!.trim());
    }
    assertTrue(!!sixPackKeyRow.shadowRoot!.querySelector('#sixPackKeyRow'));
  });

  test('Six pack key row displays correct menu options', async () => {
    await initializeKeyboardSixPackKeyRow();
    for (const key
             of [SixPackKey.DELETE, SixPackKey.INSERT, SixPackKey.PAGE_UP,
                 SixPackKey.PAGE_DOWN, SixPackKey.HOME, SixPackKey.END]) {
      await setKey((key as SixPackKey));
      assertDeepEquals(
          getMenuOptionsForSixPackKey((key as SixPackKey)),
          sixPackKeyRow.shadowRoot!
              .querySelector<SettingsDropdownMenuElement>(
                  '#keyDropdown')!.menuOptions);
    }
  });
});
