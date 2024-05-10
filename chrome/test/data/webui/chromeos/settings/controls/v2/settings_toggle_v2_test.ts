// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsToggleV2Element} from 'chrome://os-settings/os_settings.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

/** @fileoverview Suite of tests for settings-toggle-v2. */
suite('SettingsToggleV2', () => {
  let toggleElement: SettingsToggleV2Element;
  let fakeTogglePref: chrome.settingsPrivate.PrefObject;

  function init() {
    clearBody();
    toggleElement = document.createElement('settings-toggle-v2');
    document.body.appendChild(toggleElement);
  }

  async function initWithPref(prefValue: boolean = false) {
    init();

    /**
     * Pref value used in tests, should reflect the 'checked' attribute.
     * Create a new pref for each test() to prevent order (state)
     * dependencies between tests.
     */
    fakeTogglePref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: prefValue,
    };

    toggleElement.pref = {...fakeTogglePref};
    await flushTasks();
  }

  function getInternalToggle(): CrToggleElement {
    const internalToggle = toggleElement.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!internalToggle);
    assertTrue(isVisible(internalToggle));
    return internalToggle;
  }

  test('disabled toggle', () => {
    init();

    // `disabled` is false by default.
    assertFalse(toggleElement.hasAttribute('disabled'));

    toggleElement.disabled = true;
    assertTrue(toggleElement.hasAttribute('disabled'));

    // clicking a disabled toggle does not change its checked value.
    toggleElement.click();
    assertTrue(toggleElement.hasAttribute('disabled'));
    assertFalse(toggleElement.checked);

    toggleElement.disabled = false;
    assertFalse(toggleElement.hasAttribute('disabled'));
  });

  test(
      'triggers a change event when the value of toggle changes.', async () => {
        const checkedChangeEventPromise = eventToPromise('change', window);

        toggleElement.click();

        const event = await checkedChangeEventPromise;
        assertEquals(toggleElement.checked, event.detail);
      });

  test('the internal cr-toggle control value changes on click', () => {
    init();

    assertFalse(toggleElement.checked);
    const internalToggle = getInternalToggle();

    toggleElement.click();
    assertTrue(toggleElement.checked);
    assertTrue(internalToggle.checked);

    toggleElement.click();
    assertFalse(toggleElement.checked);
    assertFalse(internalToggle.checked);
  });

  test('should focus the internal toggle', () => {
    init();
    const internalToggle = getInternalToggle();

    assertNotEquals(internalToggle, toggleElement.shadowRoot!.activeElement);
    toggleElement.focus();
    assertEquals(internalToggle, toggleElement.shadowRoot!.activeElement);
  });

  suite('with pref object', () => {
    setup(async () => {
      await initWithPref();
    });

    suite('Pref type validation', () => {
      [{
        prefType: chrome.settingsPrivate.PrefType.STRING,
        testValue: 'foo',
        isValid: false,
      },
       {
         prefType: chrome.settingsPrivate.PrefType.NUMBER,
         testValue: 1,
         isValid: false,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.DICTIONARY,
         testValue: {},
         isValid: false,
       },
       {
         prefType: chrome.settingsPrivate.PrefType.BOOLEAN,
         testValue: true,
         isValid: true,
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
                toggleElement.pref = {
                  key: 'settings.sample',
                  type: prefType,
                  value: testValue,
                };
                toggleElement.validatePref();
              }

              if (isValid) {
                validatePref();
              } else {
                assertThrows(validatePref);
              }
            });
      });
    });

    test('checked value reflects the pref value', async () => {
      await initWithPref(true);

      assertTrue(toggleElement.pref!.value);
      assertTrue(toggleElement.checked);

      const internalToggle = getInternalToggle();
      assertTrue(internalToggle.checked);
    });

    test('checked value reflects the pref value when pref changes', () => {
      assertFalse(toggleElement.pref!.value);
      assertFalse(toggleElement.checked);


      toggleElement.set('pref.value', true);
      assertTrue(toggleElement.pref!.value);
      assertTrue(toggleElement.checked);
    });

    test('pref value changes on click', () => {
      assertFalse(toggleElement.checked);
      assertFalse(toggleElement.pref!.value);

      toggleElement.click();
      assertTrue(toggleElement.checked);
      assertTrue(toggleElement.pref!.value);

      toggleElement.click();
      assertFalse(toggleElement.checked);
      assertFalse(toggleElement.pref!.value);
    });

    test(
        'resetToPrefValue changes the checked property to the pref value',
        () => {
          assertFalse(toggleElement.checked);
          assertFalse(toggleElement.pref!.value);

          toggleElement.checked = true;
          assertTrue(toggleElement.checked);
          assertFalse(toggleElement.pref!.value);

          toggleElement.resetToPrefValue();
          assertFalse(toggleElement.checked);
          assertFalse(toggleElement.pref!.value);
        });

    test('toggling dispatches pref change event', async () => {
      const prefChangeEventPromise =
          eventToPromise('user-action-setting-pref-change', window);

      toggleElement.click();
      assertTrue(toggleElement.checked);
      assertEquals(toggleElement.checked, toggleElement.pref!.value);

      const event = await prefChangeEventPromise;
      assertEquals(fakeTogglePref.key, event.detail.prefKey);
      assertEquals(toggleElement.checked, event.detail.value);
    });

    test(
        'changing the pref changes the checked value when the toggle is disabled',
        () => {
          // `disabled` is false by default.
          assertFalse(toggleElement.hasAttribute('disabled'));

          toggleElement.disabled = true;
          assertTrue(toggleElement.hasAttribute('disabled'));

          // changing the pref changes the checked value
          toggleElement.set('pref.value', true);
          assertTrue(toggleElement.pref!.value);
          assertTrue(toggleElement.checked);
        });

    suite('when noSetPref is true', () => {
      setup(async () => {
        toggleElement.noSetPref = true;
      });

      test('toggling does not change the pref value', () => {
        assertFalse(toggleElement.checked);
        assertFalse(toggleElement.pref!.value);

        toggleElement.click();
        assertTrue(toggleElement.checked);
        assertFalse(toggleElement.pref!.value);

        toggleElement.click();
        assertFalse(toggleElement.checked);
        assertFalse(toggleElement.pref!.value);
      });

      test(
          'calling commitPrefChange changes the pref property to new value',
          () => {
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            toggleElement.checked = true;
            assertTrue(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            toggleElement.commitPrefChange(true);
            assertTrue(toggleElement.checked);
            assertTrue(toggleElement.pref!.value);
          });

      test(
          'calling resetToPrefValue changes the checked property to the pref value',
          () => {
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            toggleElement.checked = true;
            assertTrue(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            const internalToggle = getInternalToggle();
            assertTrue(internalToggle.checked);

            toggleElement.resetToPrefValue();
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);
          });
    });
  });

  suite('without pref object', () => {
    setup(() => {
      init();

      // there is no pref set.
      assertEquals(toggleElement.pref, undefined);
    });

    test('value changes on click', () => {
      assertFalse(toggleElement.checked);

      toggleElement.click();
      assertTrue(toggleElement.checked);

      toggleElement.click();
      assertFalse(toggleElement.checked);
    });

    test('control value changes on toggle element change', () => {
      assertFalse(toggleElement.checked);
      const internalToggle = getInternalToggle();

      toggleElement.checked = true;
      assertTrue(toggleElement.checked);
      assertTrue(internalToggle.checked);

      toggleElement.checked = false;
      assertFalse(toggleElement.checked);
      assertFalse(internalToggle.checked);
    });

    test('commitPrefChange has no effect when there is no pref', () => {
      assertFalse(toggleElement.checked);

      assertThrows(() => {
        toggleElement.commitPrefChange(true);
      }, 'updatePrefValueFromUserAction() requires pref to be defined.');

      assertFalse(toggleElement.checked);
    });

    test('resetToPrefValue has no effect when there is no pref', () => {
      assertFalse(toggleElement.checked);

      assertThrows(() => {
        toggleElement.resetToPrefValue();
      }, 'resetToPrefValue() requires pref to be defined.');

      assertFalse(toggleElement.checked);
    });
  });
});
