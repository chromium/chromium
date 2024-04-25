// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsDropdownV2Element} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

suite('<settings-dropdown-v2>', () => {
  let dropdownElement: SettingsDropdownV2Element;
  let internalSelectElement: HTMLSelectElement;

  const testOptions = [
    {label: 'Lion', value: 1},
    {label: 'Tiger', value: 2},
    {label: 'Bear', value: 3},
  ];

  const fakeNumberPrefObject = {
    key: 'settings.animal',
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: 1,
  };

  function simulateSelectAction(value: string|number): void {
    internalSelectElement.value = value.toString();
    internalSelectElement.dispatchEvent(new CustomEvent('change'));
  }

  function assertOptionSelected(expectedValue: string|number): void {
    assertEquals(expectedValue.toString(), internalSelectElement.value);

    const selectedOption =
        Array.from(internalSelectElement.options).find((option) => {
          return expectedValue.toString() === option.value;
        });

    assertTrue(
        !!selectedOption, `No matching option for value: ${expectedValue}`);
    assertTrue(selectedOption.selected, 'Option is not selected');
  }

  setup(async () => {
    clearBody();
    dropdownElement = document.createElement(SettingsDropdownV2Element.is);
    dropdownElement.options = testOptions;
    document.body.appendChild(dropdownElement);
    internalSelectElement = dropdownElement.$.select;
    await flushTasks();
  });

  suite('disabled property', () => {
    test('should reflect to attribute', () => {
      // `disabled` is false by default.
      assertFalse(dropdownElement.hasAttribute('disabled'));

      dropdownElement.disabled = true;
      assertTrue(dropdownElement.hasAttribute('disabled'));

      dropdownElement.disabled = false;
      assertFalse(dropdownElement.hasAttribute('disabled'));
    });

    test('is true if pref is enforced', () => {
      dropdownElement.pref = {...fakeNumberPrefObject};
      assertFalse(dropdownElement.disabled);

      dropdownElement.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(dropdownElement.disabled);
    });

    test('cannot be overridden if pref is enforced', () => {
      dropdownElement.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(dropdownElement.disabled);

      // Attempt to force enable the element. Element should still be disabled
      // since the pref is enforced.
      dropdownElement.disabled = false;
      assertTrue(dropdownElement.disabled);
    });
  });

  test('Internal select is disabled if there are no menu options', () => {
    assertTrue(dropdownElement.options.length > 0);
    assertFalse(internalSelectElement.disabled);

    dropdownElement.options = [];
    assertTrue(internalSelectElement.disabled);
  });

  test('Hidden options are not shown', async () => {
    const testOptionsWithHiddenAttr = [
      {label: 'Lion', value: 1, hidden: false},
      {
        label: 'Tiger',
        value: 2,
        hidden: true,
      },
      {label: 'Bear', value: 3},
    ];
    dropdownElement.options = testOptionsWithHiddenAttr;
    await flushTasks();

    testOptionsWithHiddenAttr.forEach((option, index) => {
      assertEquals(
          !!option.hidden, internalSelectElement.options[index]!.hidden);
    });
  });

  test('Not found option is included and hidden', () => {
    const options = Array.from(internalSelectElement.options);
    assertEquals(testOptions.length + 1, options.length);
    const notFoundOption = options.find((option) => {
      return option.value === 'SETTINGS_DROPDOWN_NOT_FOUND';
    });
    assertTrue(!!notFoundOption);
    assertTrue(notFoundOption.hidden);
  });

  suite(
      'validation',
      () => {
          // TODO(b/333454399) Add pref type validation tests.
      });

  suite('with pref', () => {
    setup(async () => {
      dropdownElement.pref = {...fakeNumberPrefObject};
      await flushTasks();
    });

    test('Pref value updates selected option', () => {
      assertOptionSelected(1);
      dropdownElement.set('pref.value', 2);
      assertOptionSelected(2);
      dropdownElement.set('pref.value', 3);
      assertOptionSelected(3);
    });

    test(
        'Not found option is selected if no matching option for pref value',
        () => {
          assertOptionSelected(1);
          dropdownElement.set('pref.value', 9001);
          assertOptionSelected('SETTINGS_DROPDOWN_NOT_FOUND');
          dropdownElement.set('pref.value', 2);
          assertOptionSelected(2);
        });

    test('Selecting an option updates local pref value', () => {
      for (const testOption of testOptions) {
        const value = testOption.value;
        simulateSelectAction(value);
        assertOptionSelected(value);
        assertEquals(value, dropdownElement.pref!.value);
      }
    });

    test('Selecting an option dispatches pref change event', async () => {
      for (const testOption of testOptions) {
        const prefChangeEventPromise =
            eventToPromise('user-action-setting-pref-change', window);
        const value = testOption.value;
        simulateSelectAction(value);
        assertOptionSelected(value);

        const event = await prefChangeEventPromise;
        assertEquals(fakeNumberPrefObject.key, event.detail.prefKey);
        assertEquals(value, event.detail.value);
      }
    });

    test('Selecting an option dispatches change event', async () => {
      for (const testOption of testOptions) {
        const changeEventPromise = eventToPromise('change', window);
        const value = testOption.value;
        simulateSelectAction(value);
        assertOptionSelected(value);

        const event = await changeEventPromise;
        assertEquals(value, event.detail);
      }
    });
  });

  suite('focus()', () => {
    test('should focus the internal select element', () => {
      assertNotEquals(
          internalSelectElement, dropdownElement.shadowRoot!.activeElement);
      dropdownElement.focus();
      assertEquals(
          internalSelectElement, dropdownElement.shadowRoot!.activeElement);
    });
  });
});
