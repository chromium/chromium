// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsDropdownV2Element} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

suite(SettingsDropdownV2Element.is, () => {
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

    const selectedOptionIndex =
        Array.from(internalSelectElement.options).findIndex((option) => {
          return expectedValue.toString() === option.value;
        });
    assertEquals(selectedOptionIndex, internalSelectElement.selectedIndex);

    const selectedOption = internalSelectElement.options[selectedOptionIndex];
    assertTrue(
        !!selectedOption, `No matching option for value: ${expectedValue}`);
    assertTrue(selectedOption.selected, 'Option is not selected');
  }

  function assertNoOptionSelected(): void {
    assertEquals('', internalSelectElement.value);
    assertEquals(-1, internalSelectElement.selectedIndex);
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

  suite('with pref', () => {
    setup(async () => {
      dropdownElement.pref = {...fakeNumberPrefObject};
      await flushTasks();
    });

    suite('Pref type validation', () => {
      [{
        prefType: chrome.settingsPrivate.PrefType.STRING,
        testValue: 'foo',
        isValid: true,
      },
       {
         prefType: chrome.settingsPrivate.PrefType.NUMBER,
         testValue: 1,
         isValid: true,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.DICTIONARY,
         testValue: {},
         isValid: false,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.BOOLEAN,
         testValue: true,
         isValid: false,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.LIST,
         testValue: [],
         isValid: false,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.URL,
         testValue: 'bar',
         isValid: false,
       },
      ].forEach(({prefType, testValue, isValid}) => {
        test(
            `${prefType} pref type is ${isValid ? 'valid' : 'invalid'}`, () => {
              function validatePref() {
                dropdownElement.pref = {
                  key: 'settings.sample',
                  type: prefType,
                  value: testValue,
                };
                dropdownElement.validatePref();
              }

              if (isValid) {
                validatePref();
              } else {
                assertThrows(validatePref);
              }
            });
      });
    });

    test('Pref value updates the selected option', async () => {
      for (const testOption of testOptions) {
        dropdownElement.set('pref.value', testOption.value);
        await flushTasks();
        assertOptionSelected(testOption.value);
      }
    });

    test(
        'No option is selected if no matching option for pref value',
        async () => {
          assertOptionSelected(1);

          dropdownElement.set('pref.value', 9001);
          await flushTasks();
          assertNoOptionSelected();

          dropdownElement.set('pref.value', 2);
          await flushTasks();
          assertOptionSelected(2);
        });

    test('Pref value syncs to the "value" property', () => {
      dropdownElement.set('pref.value', 2);
      assertEquals(2, dropdownElement.value);

      dropdownElement.set('pref.value', 3);
      assertEquals(3, dropdownElement.value);

      dropdownElement.set('pref.value', 9001);
      assertEquals(9001, dropdownElement.value);
    });

    test('policy indicator does not show if pref is not enforced', () => {
      const policyIndicator =
          dropdownElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
      assertFalse(isVisible(policyIndicator));
    });

    test('policy indicator shows if pref is enforced', async () => {
      dropdownElement.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      };
      await flushTasks();

      const policyIndicator =
          dropdownElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
      assertTrue(isVisible(policyIndicator));
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

    test(
        'Selecting an option does not update the pref value directly',
        async () => {
          const initialPrefValue = dropdownElement.pref!.value;

          const prefChangeEventPromise =
              eventToPromise('user-action-setting-pref-change', window);
          const value = testOptions[0]!.value;
          simulateSelectAction(value);
          assertOptionSelected(value);
          await prefChangeEventPromise;

          // Local pref object should be treated as immutable data and should
          // not be updated directly.
          assertEquals(initialPrefValue, dropdownElement.pref!.value);
        });

    test('Selecting an option dispatches change event', async () => {
      for (const testOption of testOptions) {
        const value = testOption.value;
        const changeEventPromise = eventToPromise('change', window);
        simulateSelectAction(value);
        assertOptionSelected(value);
        const event = await changeEventPromise;
        assertEquals(value, event.detail);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
      }
    });
  });

  suite('without pref', () => {
    test('Changing value updates the selected option', async () => {
      for (const testOption of testOptions) {
        dropdownElement.value = testOption.value;
        await flushTasks();
        assertOptionSelected(testOption.value);
      }
    });

    test('No option is selected if no matching option for value', async () => {
      assertNoOptionSelected();

      dropdownElement.value = 1;
      await flushTasks();
      assertOptionSelected(1);

      dropdownElement.value = 9001;
      await flushTasks();
      assertNoOptionSelected();
    });

    test('Selecting an option updates value property', () => {
      for (const testOption of testOptions) {
        const value = testOption.value;
        simulateSelectAction(value);
        assertOptionSelected(value);
        assertEquals(value, dropdownElement.value);
      }
    });

    test('Selecting an option dispatches change event', async () => {
      for (const testOption of testOptions) {
        const value = testOption.value;
        const changeEventPromise = eventToPromise('change', window);
        simulateSelectAction(value);
        assertOptionSelected(value);
        const event = await changeEventPromise;
        assertEquals(value, event.detail);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
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

  suite('for a11y', () => {
    test('ariaLabel property should apply to internal select', () => {
      const ariaLabel = 'A11y label';
      dropdownElement.ariaLabel = ariaLabel;
      assertEquals(ariaLabel, internalSelectElement.getAttribute('aria-label'));
    });

    test('ariaLabel property does not reflect to attribute', () => {
      const ariaLabel = 'A11y label';
      dropdownElement.ariaLabel = ariaLabel;
      assertFalse(dropdownElement.hasAttribute('aria-label'));
    });

    test('ariaDescription property should apply to internal select', () => {
      const ariaDescription = 'A11y description';
      dropdownElement.ariaDescription = ariaDescription;
      assertEquals(
          ariaDescription,
          internalSelectElement.getAttribute('aria-description'));
    });

    test('ariaDescription property does not reflect to attribute', () => {
      const ariaDescription = 'A11y description';
      dropdownElement.ariaDescription = ariaDescription;
      assertFalse(dropdownElement.hasAttribute('aria-description'));
    });
  });
});
