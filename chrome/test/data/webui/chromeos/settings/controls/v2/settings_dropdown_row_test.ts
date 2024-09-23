// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsDropdownRowElement, SettingsDropdownV2Element, SettingsRowElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

suite(SettingsDropdownRowElement.is, () => {
  let dropdownRow: SettingsDropdownRowElement;
  let internalRowElement: SettingsRowElement;
  let internalDropdownElement: SettingsDropdownV2Element;
  let internalSelectElement: HTMLSelectElement;

  const testOptions = [
    {label: 'Lion', value: 1},
    {label: 'Tiger', value: 2},
    {label: 'Bear', value: 3},
    {label: 'Dragon', value: 4},
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
    assertEquals(expectedValue, internalDropdownElement.value);
  }

  setup(async () => {
    clearBody();
    dropdownRow = document.createElement(SettingsDropdownRowElement.is);
    dropdownRow.options = testOptions;
    document.body.appendChild(dropdownRow);

    internalRowElement = dropdownRow.$.internalRow;
    internalDropdownElement = dropdownRow.$.dropdown;
    internalSelectElement = internalDropdownElement.$.select;
    await flushTasks();
  });

  suite('for internal row element', () => {
    test('internal dropdown element is slotted into the control slot', () => {
      const slotEl = strictQuery(
          'slot[name="control"]', internalRowElement.shadowRoot,
          HTMLSlotElement);
      const slottedElements = slotEl.assignedElements({flatten: true});
      assertEquals(1, slottedElements.length);
      assertEquals(internalDropdownElement, slottedElements[0]);
    });

    test('label is passed to internal row', () => {
      const label = 'Lorem ipsum';
      dropdownRow.label = label;
      assertEquals(label, internalRowElement.label);
    });

    test('sublabel is passed to internal row', () => {
      const sublabel = 'Lorem ipsum dolor sit amet';
      dropdownRow.sublabel = sublabel;
      assertEquals(sublabel, internalRowElement.sublabel);
    });

    test('icon is passed to internal row', () => {
      const icon = 'os-settings:display';
      dropdownRow.icon = icon;
      assertEquals(icon, internalRowElement.icon);
    });

    test('learn more URL is passed to internal row', () => {
      const learnMoreUrl = 'https://google.com';
      dropdownRow.learnMoreUrl = learnMoreUrl;
      assertEquals(learnMoreUrl, internalRowElement.learnMoreUrl);
    });
  });

  suite('with disabled property', () => {
    test('should reflect to attribute', () => {
      // `disabled` is false by default.
      assertFalse(dropdownRow.hasAttribute('disabled'));

      dropdownRow.disabled = true;
      assertTrue(dropdownRow.hasAttribute('disabled'));

      dropdownRow.disabled = false;
      assertFalse(dropdownRow.hasAttribute('disabled'));
    });

    test('is true if pref is enforced', () => {
      dropdownRow.pref = {...fakeNumberPrefObject};
      assertFalse(dropdownRow.disabled);

      dropdownRow.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(dropdownRow.disabled);
    });

    test('cannot be overridden if pref is enforced', () => {
      dropdownRow.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(dropdownRow.disabled);

      // Attempt to force enable the element. Element should still be disabled
      // since the pref is enforced.
      dropdownRow.disabled = false;
      assertTrue(dropdownRow.disabled);
    });

    test('internal dropdown is disabled', () => {
      assertFalse(internalDropdownElement.disabled);

      dropdownRow.disabled = true;
      assertTrue(internalDropdownElement.disabled);
    });
  });

  suite('with pref', () => {
    setup(async () => {
      dropdownRow.pref = {...fakeNumberPrefObject};
      await flushTasks();
    });

    test('pref value updates the selected option', async () => {
      for (const testOption of testOptions) {
        dropdownRow.set('pref.value', testOption.value);
        await flushTasks();
        assertOptionSelected(testOption.value);
      }
    });

    test('policy indicator shows if pref is enforced', async () => {
      dropdownRow.pref = {
        ...fakeNumberPrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      };
      await flushTasks();

      const policyIndicator = internalDropdownElement.shadowRoot!.querySelector(
          'cr-policy-pref-indicator');
      assertTrue(isVisible(policyIndicator));
    });

    test('selecting an option dispatches pref change event', async () => {
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

    test('selecting an option dispatches change event', async () => {
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
    test('changing value updates the selected option', async () => {
      for (const testOption of testOptions) {
        dropdownRow.value = testOption.value;
        await flushTasks();
        assertOptionSelected(testOption.value);
      }
    });

    test('selecting an option dispatches change event', async () => {
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
      assertNotEquals(internalSelectElement, getDeepActiveElement());
      dropdownRow.focus();
      assertEquals(internalSelectElement, getDeepActiveElement());
    });
  });

  suite('for a11y', () => {
    test('label is the ARIA label by default', () => {
      const label = 'Lorem ipsum';
      dropdownRow.label = label;
      assertEquals(label, internalSelectElement.getAttribute('aria-label'));
    });

    test('sublabel is the ARIA description by default', () => {
      const sublabel = 'Lorem ipsum dolor sit amet';
      dropdownRow.sublabel = sublabel;
      assertEquals(
          sublabel, internalSelectElement.getAttribute('aria-description'));
    });

    test('ariaLabel property takes precedence for the ARIA label', () => {
      const label = 'Lorem ipsum';
      const ariaLabel = 'A11y ' + label;
      dropdownRow.label = label;
      dropdownRow.ariaLabel = ariaLabel;
      assertEquals(ariaLabel, internalSelectElement.getAttribute('aria-label'));
    });

    test(
        'ariaDescription property takes precedence for the ARIA description',
        () => {
          const sublabel = 'Lorem ipsum dolor sit amet';
          const ariaDescription = 'A11y ' + sublabel;
          dropdownRow.sublabel = sublabel;
          dropdownRow.ariaDescription = ariaDescription;
          assertEquals(
              ariaDescription,
              internalSelectElement.getAttribute('aria-description'));
        });
  });
});
