// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonRowElement, CustomizeButtonSelectElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, fakeMice, fakeMouseButtonActions, getInputDeviceSettingsProvider, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-row>', () => {
  let customizeButtonRow: CustomizeButtonRowElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(() => {
    setupFakeInputDeviceSettingsProvider();
    provider =
        getInputDeviceSettingsProvider() as FakeInputDeviceSettingsProvider;

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

  async function initializeCustomizeButtonRow() {
    customizeButtonRow = document.createElement(CustomizeButtonRowElement.is);
    customizeButtonRow.set('actionList', fakeGraphicsTabletButtonActions);
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();

    document.body.appendChild(customizeButtonRow);
    customizeButtonRow.blur();
    return flushTasks();
  }

  async function initializeMouseCustomizeButtonRow() {
    customizeButtonRow = document.createElement(CustomizeButtonRowElement.is);
    customizeButtonRow.set('actionList', fakeMouseButtonActions);
    customizeButtonRow.set(
        'buttonRemappingList', fakeMice[0]!.settings.buttonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();

    document.body.appendChild(customizeButtonRow);
    return flushTasks();
  }

  function getSelectedValue(): string {
    const dropdown: CustomizeButtonSelectElement|null =
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown');
    assertTrue(!!dropdown);
    return dropdown!.get('selectedValue');
  }

  function getSelectDropdownElement() {
    const dropdownElement =
        customizeButtonRow.$.remappingActionDropdown!.$.selectDropdown;
    return dropdownElement;
  }

  test('Initialize mouse customize button row', async () => {
    await initializeMouseCustomizeButtonRow();
    let expectedRemapping = fakeMice[0]!.settings.buttonRemappings[0];
    assertDeepEquals(
        customizeButtonRow.get('buttonRemapping_'), expectedRemapping);
    assertEquals(
        customizeButtonRow.shadowRoot!.querySelector(
                                          '#buttonLabel')!.textContent,
        expectedRemapping!.name);
    assertEquals(
        getSelectedValue(),
        'staticShortcutAction' +
            expectedRemapping!.remappingAction?.staticShortcutAction!
                .toString());

    // Change buttonRemapping data to display.
    customizeButtonRow.set('remappingIndex', 1);
    customizeButtonRow.set(
        'buttonRemappingList', fakeMice[0]!.settings.buttonRemappings);
    await flushTasks();
    expectedRemapping = fakeMice[0]!.settings.buttonRemappings[1];
    assertEquals(
        customizeButtonRow.shadowRoot!.querySelector(
                                          '#buttonLabel')!.textContent,
        expectedRemapping!.name);
    assertEquals(
        getSelectedValue(),
        'acceleratorAction' +
            expectedRemapping!.remappingAction?.acceleratorAction!.toString());
  });

  test('Focus current row correct button', async () => {
    await initializeCustomizeButtonRow();
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();

    assertNotEquals(
        getDeepActiveElement(),
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown'));
    provider.sendButtonPress(
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings[0]!.button);
    await flushTasks();

    assertEquals(
        getDeepActiveElement(),
        // customizeButtonRow.shadowRoot!.querySelector(
        //     '#remappingActionDropdown'))
        getSelectDropdownElement());
  });

  test('Focus row wrong button, not focused', async () => {
    await initializeCustomizeButtonRow();
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();

    assertNotEquals(
        getDeepActiveElement(),
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown'));
    provider.sendButtonPress(
        fakeGraphicsTablets[0]!.settings.tabletButtonRemappings[1]!.button);
    await flushTasks();

    assertNotEquals(
        getDeepActiveElement(),
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown'));
  });
});
