// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsDropdownMenuElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

/** @fileoverview Suite of tests for settings-dropdown-menu. */
suite('SettingsDropdownMenu', function() {
  let dropdown: SettingsDropdownMenuElement;

  // The <select> used internally by the dropdown menu.
  let selectElement: HTMLSelectElement;

  // The "Custom" option in the <select> menu.
  let customOption: HTMLOptionElement;

  function waitUntilDropdownUpdated(): Promise<void> {
    return waitAfterNextRender(dropdown);
  }

  function simulateChangeEvent(value: string): Promise<void> {
    selectElement.value = value;
    selectElement.dispatchEvent(new CustomEvent('change'));
    return waitUntilDropdownUpdated();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('settings-dropdown-menu');
    document.body.appendChild(dropdown);
    selectElement = dropdown.shadowRoot!.querySelector('select')!;
    assertTrue(!!selectElement);
    const options = selectElement.options;
    customOption = options[options.length - 1]!;
    assertTrue(!!customOption);
  });

  test('with number options', async function() {
    dropdown.pref = {
      key: 'test.number',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 100,
    };
    dropdown.menuOptions = [
      {value: 100, name: 'Option 100'},
      {value: 200, name: 'Option 200'},
      {value: 300, name: 'Option 300'},
      {value: 400, name: 'Option 400'},
    ];
    await waitUntilDropdownUpdated();

    // Initially selected item.
    assertEquals(
        'Option 100', selectElement.selectedOptions[0]!.textContent!.trim());

    // Selecting an item updates the pref.
    await simulateChangeEvent('200');
    assertEquals(200, dropdown.pref!.value);
    assertEquals('200', dropdown.getSelectedValue());

    // Updating the pref selects an item.
    dropdown.set('pref.value', 400);
    await waitUntilDropdownUpdated();
    assertEquals('400', selectElement.value);
    assertEquals('400', dropdown.getSelectedValue());
  });

  test('with string options', async function() {
    dropdown.pref = {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'c',
    };
    dropdown.menuOptions = [
      {value: 'a', name: 'AAA'},
      {value: 'b', name: 'BBB'},
      {value: 'c', name: 'CCC'},
      {value: 'd', name: 'DDD'},
    ];
    await waitUntilDropdownUpdated();

    // Initially selected item.
    assertEquals('CCC', selectElement.selectedOptions[0]!.textContent!.trim());

    // Selecting an item updates the pref.
    await simulateChangeEvent('a');
    assertEquals('a', dropdown.pref!.value);
    assertEquals('a', dropdown.getSelectedValue());

    // Item remains selected after updating menu items.
    const newMenuOptions = dropdown.menuOptions.slice().reverse();
    dropdown.menuOptions = newMenuOptions;
    await waitUntilDropdownUpdated();
    assertEquals('AAA', selectElement.selectedOptions[0]!.textContent!.trim());
    assertEquals('AAA', selectElement.selectedOptions[0]!.textContent!.trim());
  });

  test('with noSetPref', async function() {
    dropdown.noSetPref = true;
    dropdown.pref = {
      key: 'test.number',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 100,
    };
    dropdown.menuOptions = [
      {value: 100, name: 'Option 100'},
      {value: 200, name: 'Option 200'},
      {value: 300, name: 'Option 300'},
    ];
    await waitUntilDropdownUpdated();

    // Initially selected item.
    assertEquals(
        'Option 100', selectElement.selectedOptions[0]!.textContent!.trim());

    // Updating the pref selects an item also with noSetPref.
    dropdown.set('pref.value', 200);
    await waitUntilDropdownUpdated();
    assertEquals('200', selectElement.value);
    assertEquals('200', dropdown.getSelectedValue());

    // Selecting an item does not automatically update the pref with noSetPref.
    await simulateChangeEvent('300');
    assertEquals(200, dropdown.pref!.value);
    assertEquals('300', dropdown.getSelectedValue());

    // Calling |sendPrefChange()| updates the pref.
    dropdown.sendPrefChange();
    assertEquals(300, dropdown.pref!.value);
  });

  test('with custom value', async function() {
    dropdown.pref = {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'f',
    };
    dropdown.menuOptions = [
      {value: 'a', name: 'AAA'},
      {value: 'b', name: 'BBB'},
      {value: 'c', name: 'CCC'},
      {value: 'd', name: 'DDD'},
    ];
    dropdown.addEventListener('settings-control-change', () => {
      // Failure, custom value shouldn't ever call this.
      assertNotReached(
          'settings-control-change should not be triggered for custom value');
    });

    await waitUntilDropdownUpdated();
    // "Custom" initially selected.
    assertEquals(dropdown.notFoundValue, selectElement.value);
    assertEquals(dropdown.notFoundValue, dropdown.getSelectedValue());
    assertEquals('block', getComputedStyle(customOption).display);
    assertFalse(customOption.disabled);

    // Pref should not have changed.
    assertEquals('f', dropdown.pref!.value);
  });

  test('with hidden options', async function() {
    dropdown.pref = {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'f',
    };
    dropdown.menuOptions = [
      {value: 'a', name: 'AAA', hidden: true},
      {value: 'b', name: 'BBB', hidden: false},
      {value: 'c', name: 'CCC'},
      {value: 'd', name: 'DDD'},
    ];
    await waitUntilDropdownUpdated();

    // `options` contains the options above plus 'Custom'.
    assertEquals(5, selectElement.options.length);
    assertTrue(selectElement.options[0]!.hidden);
    assertFalse(selectElement.options[1]!.hidden);
    assertFalse(selectElement.options[2]!.hidden);
    assertFalse(selectElement.options[3]!.hidden);
  });

  function waitForTimeout(timeMs: number): Promise<void> {
    return new Promise<void>(function(resolve) {
      setTimeout(resolve, timeMs);
    });
  }

  test('delay setting options', async function() {
    dropdown.pref = {
      key: 'test.number2',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 200,
    };

    await waitForTimeout(100);
    await waitUntilDropdownUpdated();
    assertTrue(selectElement.disabled);
    assertEquals('SETTINGS_DROPDOWN_NOT_FOUND_ITEM', selectElement.value);

    dropdown.menuOptions = [
      {value: 100, name: 'Option 100'},
      {value: 200, name: 'Option 200'},
      {value: 300, name: 'Option 300'},
      {value: 400, name: 'Option 400'},
    ];
    await waitUntilDropdownUpdated();
    // Dropdown menu enables itself and selects the new menu option
    // corresponding to the pref value.
    assertFalse(selectElement.disabled);
    assertEquals('200', selectElement.value);

    // The "Custom" option should not show up in the dropdown list or be
    // reachable via type-ahead.
    assertEquals('none', getComputedStyle(customOption).display);
    assertTrue(customOption.disabled);
  });

  test('works with dictionary pref', async function() {
    let settingsControlChangeCount = 0;
    dropdown.pref = {
      key: 'test.dictionary',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {
        'key1': 'value1',
        'key2': 'value2',
      },
    };
    dropdown.prefKey = 'key2';
    dropdown.menuOptions = [
      {value: 'value2', name: 'Option 2'},
      {value: 'value3', name: 'Option 3'},
      {value: 'value4', name: 'Option 4'},
    ];
    dropdown.addEventListener('settings-control-change', () => {
      ++settingsControlChangeCount;
    });

    await waitUntilDropdownUpdated();
    // Initially selected item.
    assertEquals(
        'Option 2', selectElement.selectedOptions[0]!.textContent!.trim());

    // Setup does not call the settings-control-change event.
    assertEquals(0, settingsControlChangeCount);

    // Selecting an item updates the pref.
    await simulateChangeEvent('value3');
    assertEquals('value3', dropdown.pref!.value['key2']);

    // The settings-control-change callback should have been triggered
    // exactly once.
    assertEquals(1, settingsControlChangeCount);

    // Updating the pref selects an item.
    dropdown.set('pref.value.key2', 'value4');
    await waitUntilDropdownUpdated();
    assertEquals('value4', selectElement.value);

    // The settings-control-change callback should have been triggered
    // exactly once still -- manually updating the pref is not a user
    // action so the count should not be incremented.
    assertEquals(1, settingsControlChangeCount);
  });
});
