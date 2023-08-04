// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonRowElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTablets, SettingsDropdownMenuElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-row>', () => {
  let customizeButtonRow: CustomizeButtonRowElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!customizeButtonRow) {
      return;
    }
    customizeButtonRow.remove();
    await flushTasks();
  });

  function initializeCustomizeButtonRow() {
    customizeButtonRow = document.createElement(CustomizeButtonRowElement.is);
    document.body.appendChild(customizeButtonRow);
    return flushTasks();
  }

  function getSelectedValue(): string {
    const dropdown: SettingsDropdownMenuElement|null =
        customizeButtonRow.shadowRoot!.querySelector('#buttonDropdown');
    const dropdownMenu: HTMLSelectElement|null =
        dropdown!.shadowRoot!.querySelector('#dropdownMenu');
    assertTrue(!!dropdownMenu);
    return dropdownMenu!.value;
  }

  test('Initialize customize button row', async () => {
    await initializeCustomizeButtonRow();
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();
    let expectedRemapping =
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings[0];
    assertDeepEquals(
        customizeButtonRow.get('buttonRemapping_'), expectedRemapping);
    assertEquals(
        customizeButtonRow.shadowRoot!.querySelector(
                                          '#buttonLabel')!.textContent,
        expectedRemapping!.name);
    assertEquals(
        getSelectedValue(),
        expectedRemapping!.remappingAction?.action!.toString());

    // Change buttonRemapping data to display.
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[1]!.settings.tabletButtonRemappings);
    customizeButtonRow.set('remappingIndex', 1);
    await flushTasks();
    expectedRemapping =
        fakeGraphicsTablets[1]!.settings.tabletButtonRemappings[1];
    assertEquals(
        customizeButtonRow.shadowRoot!.querySelector(
                                          '#buttonLabel')!.textContent,
        expectedRemapping!.name);
    assertEquals(
        getSelectedValue(),
        expectedRemapping!.remappingAction?.action!.toString());
  });
});
