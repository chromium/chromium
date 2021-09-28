// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';

/** @fileoverview Suite of tests for settings-dropdown-menu. */
suite('SettingsDropdownMenu', function() {
  /** @type {SettingsDropdownMenu} */
  let dropdown;

  /**
   * The <select> used internally by the dropdown menu.
   * @type {HTMLSelectElement}
   */
  let selectElement;

  /**
   * The "Custom" option in the <select> menu.
   * @type {HTMLOptionElement}
   */
  let customOption;

  function waitUntilDropdownUpdated() {
    return new Promise(function(resolve) {
      dropdown.async(resolve);
    });
  }

  function simulateChangeEvent(value) {
    selectElement.value = value;
    selectElement.dispatchEvent(new CustomEvent('change'));
    return waitUntilDropdownUpdated();
  }

  setup(function() {
    PolymerTest.clearBody();
    dropdown = document.createElement('settings-dropdown-menu');
    document.body.appendChild(dropdown);
    selectElement = dropdown.$$('select');
    assertTrue(!!selectElement);
    const options = selectElement.options;
    customOption = options[options.length - 1];
    assertTrue(!!customOption);
  });

  test('with number options', function testNumberOptions() {
    dropdown.pref = {
      key: 'test.number',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 100,
    };
    dropdown.menuOptions = [
      {value: 100, name: 'Option 100'}, {value: 200, name: 'Option 200'},
      {value: 300, name: 'Option 300'}, {value: 400, name: 'Option 400'}
    ];

    return waitUntilDropdownUpdated()
        .then(function() {
          // Initially selected item.
          assertEquals(
              'Option 100',
              selectElement.selectedOptions[0].textContent.trim());

          // Selecting an item updates the pref.
          return simulateChangeEvent('200');
        })
        .then(function() {
          assertEquals(200, dropdown.pref.value);

          // Updating the pref selects an item.
          dropdown.set('pref.value', 400);
          return waitUntilDropdownUpdated();
        })
        .then(function() {
          assertEquals('400', selectElement.value);
        });
  });

  test('with string options', function testStringOptions() {
    dropdown.pref = {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'c',
    };
    dropdown.menuOptions = [
      {value: 'a', name: 'AAA'}, {value: 'b', name: 'BBB'},
      {value: 'c', name: 'CCC'}, {value: 'd', name: 'DDD'}
    ];

    return waitUntilDropdownUpdated()
        .then(function() {
          // Initially selected item.
          assertEquals(
              'CCC', selectElement.selectedOptions[0].textContent.trim());

          // Selecting an item updates the pref.
          return simulateChangeEvent('a');
        })
        .then(function() {
          assertEquals('a', dropdown.pref.value);

          // Item remains selected after updating menu items.
          const newMenuOptions = dropdown.menuOptions.slice().reverse();
          dropdown.menuOptions = newMenuOptions;
          return waitUntilDropdownUpdated();
        })
        .then(function() {
          assertEquals(
              'AAA', selectElement.selectedOptions[0].textContent.trim());
        });
  });

  test('with custom value', function testCustomValue() {
    dropdown.pref = {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'f',
    };
    dropdown.menuOptions = [
      {value: 'a', name: 'AAA'}, {value: 'b', name: 'BBB'},
      {value: 'c', name: 'CCC'}, {value: 'd', name: 'DDD'}
    ];
    dropdown.addEventListener('settings-control-change', () => {
      // Failure, custom value shouldn't ever call this.
      fail('settings-control-change should not be triggered for custom value');
    });

    return waitUntilDropdownUpdated().then(function() {
      // "Custom" initially selected.
      assertEquals(dropdown.notFoundValue_, selectElement.value);
      assertEquals('block', getComputedStyle(customOption).display);
      assertFalse(customOption.disabled);

      // Pref should not have changed.
      assertEquals('f', dropdown.pref.value);
    });
  });

  function waitForTimeout(timeMs) {
    return new Promise(function(resolve) {
      setTimeout(resolve, timeMs);
    });
  }

  test('delay setting options', function testDelayedOptions() {
    dropdown.pref = {
      key: 'test.number2',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 200,
    };

    return waitForTimeout(100)
        .then(function() {
          return waitUntilDropdownUpdated();
        })
        .then(function() {
          assertTrue(selectElement.disabled);
          assertEquals('SETTINGS_DROPDOWN_NOT_FOUND_ITEM', selectElement.value);

          dropdown.menuOptions = [
            {value: 100, name: 'Option 100'}, {value: 200, name: 'Option 200'},
            {value: 300, name: 'Option 300'}, {value: 400, name: 'Option 400'}
          ];
          return waitUntilDropdownUpdated();
        })
        .then(function() {
          // Dropdown menu enables itself and selects the new menu option
          // correpsonding to the pref value.
          assertFalse(selectElement.disabled);
          assertEquals('200', selectElement.value);

          // The "Custom" option should not show up in the dropdown list or be
          // reachable via type-ahead.
          assertEquals('none', getComputedStyle(customOption).display);
          assertTrue(customOption.disabled);
        });
  });

  test('works with dictionary pref', function testDictionaryPref() {
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

    return waitUntilDropdownUpdated()
        .then(function() {
          // Initially selected item.
          assertEquals(
              'Option 2', selectElement.selectedOptions[0].textContent.trim());

          // Setup does not call the settings-control-change event.
          assertEquals(0, settingsControlChangeCount);

          // Selecting an item updates the pref.
          return simulateChangeEvent('value3');
        })
        .then(function() {
          assertEquals('value3', dropdown.pref.value['key2']);

          // The settings-control-change callback should have been triggered
          // exactly once.
          assertEquals(1, settingsControlChangeCount);

          // Updating the pref selects an item.
          dropdown.set('pref.value.key2', 'value4');
          return waitUntilDropdownUpdated();
        })
        .then(function() {
          assertEquals('value4', selectElement.value);

          // The settings-control-change callback should have been triggered
          // exactly once still -- manually updating the pref is not a user
          // action so the count should not be incremented.
          assertEquals(1, settingsControlChangeCount);
        });
  });
});
