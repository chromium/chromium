// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonRowElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, getInputDeviceSettingsProvider, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-row>', () => {
  let customizeButtonRow: CustomizeButtonRowElement;
  let buttonRemappingChangedEventCount: number = 0;
  let showKeyCombinationDialogEventCount: number = 0;
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
    buttonRemappingChangedEventCount = 0;
    showKeyCombinationDialogEventCount = 0;
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

    customizeButtonRow.addEventListener('button-remapping-changed', function() {
      buttonRemappingChangedEventCount++;
    });
    customizeButtonRow.addEventListener(
        'show-key-combination-dialog', function() {
          showKeyCombinationDialogEventCount++;
        });
    document.body.appendChild(customizeButtonRow);
    return flushTasks();
  }

  function getSelectedValue(): string {
    const dropdown: HTMLSelectElement|null =
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown');
    assertTrue(!!dropdown);
    return dropdown!.value;
  }

  test('Initialize customize button row', async () => {
    await initializeCustomizeButtonRow();
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
    customizeButtonRow.set('remappingIndex', 1);
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[1]!.settings.tabletButtonRemappings);
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

  test('Initialize key combination string', async () => {
    await initializeCustomizeButtonRow();
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings.penButtonRemappings);
    customizeButtonRow.set('remappingIndex', 0);
    await flushTasks();

    assertEquals(getSelectedValue(), 'key combination');
    assertEquals(customizeButtonRow.get('keyCombinationLabel_'), 'ctrl + z');

    // Switch to another button remapping.
    customizeButtonRow.set(
        'buttonRemappingList',
        fakeGraphicsTablets[1]!.settings.penButtonRemappings);
    customizeButtonRow.set('remappingIndex', 1);
    await flushTasks();

    assertEquals(getSelectedValue(), 'key combination');
    assertEquals(customizeButtonRow.get('keyCombinationLabel_'), 'ctrl + v');
  });

  test('update dropdown will sent events', async () => {
    await initializeCustomizeButtonRow();

    assertEquals(getSelectedValue(), '2');
    assertEquals(buttonRemappingChangedEventCount, 0);
    // Update select to another remapping action.
    const select: HTMLSelectElement|null =
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown');
    assertTrue(!!select);
    select.value = '1';
    select.dispatchEvent(new Event('change'));
    await flushTasks();

    // Verify that event is fired and button remapping is updated.
    assertEquals(buttonRemappingChangedEventCount, 1);
    assertDeepEquals(
        customizeButtonRow.get('buttonRemapping_')?.remappingAction, {
          action: 1,
        });

    // Update select to no remapping action choice.
    select.value = 'none';
    select.dispatchEvent(new Event('change'));
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 2);
    assertEquals(
        customizeButtonRow.get('buttonRemapping_')?.remappingAction, undefined);

    // Update select from no remapping back to normal remapping action.
    select.value = '2';
    select.dispatchEvent(new Event('change'));
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 3);
    assertDeepEquals(
        customizeButtonRow.get('buttonRemapping_')?.remappingAction, {
          action: 2,
        });

    // Update select to the same action, no events will be fired.
    select.value = '2';
    select.dispatchEvent(new Event('change'));
    await flushTasks();
    assertEquals(buttonRemappingChangedEventCount, 3);
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
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown'));
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

  test('select key combination will fire open dialog event', async () => {
    await initializeCustomizeButtonRow();
    assertEquals(showKeyCombinationDialogEventCount, 0);

    const select: HTMLSelectElement|null =
        customizeButtonRow.shadowRoot!.querySelector(
            '#remappingActionDropdown');
    assertTrue(!!select);
    select.value = 'key combination';
    select.dispatchEvent(new Event('change'));

    await flushTasks();
    assertEquals(showKeyCombinationDialogEventCount, 1);
  });
});
