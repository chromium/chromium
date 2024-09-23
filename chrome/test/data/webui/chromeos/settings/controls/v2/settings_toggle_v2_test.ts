// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrToggleElement, SettingsToggleV2Element} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertFalse, assertNotEquals, assertNotReached, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

/** @fileoverview Suite of tests for settings-toggle-v2. */
suite(SettingsToggleV2Element.is, () => {
  let toggleElement: SettingsToggleV2Element;
  let internalToggleElement: CrToggleElement;
  let fakeTogglePref: chrome.settingsPrivate.PrefObject;

  async function init() {
    clearBody();
    toggleElement = document.createElement('settings-toggle-v2');
    document.body.appendChild(toggleElement);

    await flushTasks();

    internalToggleElement =
        strictQuery('cr-toggle', toggleElement.shadowRoot, CrToggleElement);
    assertTrue(isVisible(internalToggleElement));
  }

  async function initWithPref(
      prefValue: boolean = false, isInverted: boolean = false) {
    await init();

    toggleElement.inverted = isInverted;

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

  suite('when disabled', () => {
    setup(async () => {
      await init();
      toggleElement.disabled = true;
    });

    test('disabled property is reflected to attribute', () => {
      assertTrue(toggleElement.hasAttribute('disabled'));

      toggleElement.disabled = false;
      assertFalse(toggleElement.hasAttribute('disabled'));
    });

    test('internal cr-toggle is disabled', () => {
      assertTrue(internalToggleElement.disabled);
    });

    test('clicking does not change the toggle state', () => {
      assertFalse(toggleElement.checked);
      assertFalse(internalToggleElement.checked);

      internalToggleElement.click();
      assertFalse(toggleElement.checked);
      assertFalse(internalToggleElement.checked);
    });
  });

  test(
      'triggers a change event when the value of toggle changes.', async () => {
        await init();
        const checkedChangeEventPromise = eventToPromise('change', window);

        internalToggleElement.click();

        const event = await checkedChangeEventPromise;
        assertEquals(toggleElement.checked, event.detail);
        // Event should not pass the shadow DOM boundary.
        assertFalse(event.composed);
      });

  test('the internal cr-toggle control value changes on click', async () => {
    await init();

    assertFalse(toggleElement.checked);

    internalToggleElement.click();
    assertTrue(toggleElement.checked);
    assertTrue(internalToggleElement.checked);

    internalToggleElement.click();
    assertFalse(toggleElement.checked);
    assertFalse(internalToggleElement.checked);
  });

  test('should focus the internal toggle', async () => {
    await init();

    assertNotEquals(
        internalToggleElement, toggleElement.shadowRoot!.activeElement);
    toggleElement.focus();
    assertEquals(
        internalToggleElement, toggleElement.shadowRoot!.activeElement);
  });

  suite('with pref object', () => {
    setup(async () => {
      await initWithPref();
    });

    async function setEnforcedPref(): Promise<void> {
      toggleElement.pref = {
        ...fakeTogglePref,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      };

      await flushTasks();
    }

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

    test('control value reflects the pref value', async () => {
      await initWithPref(true);

      assertTrue(toggleElement.pref!.value);
      assertTrue(toggleElement.checked);

      assertTrue(internalToggleElement.checked);
    });

    test('checked property reflects the pref value when pref changes', () => {
      assertFalse(toggleElement.pref!.value);
      assertFalse(toggleElement.checked);

      toggleElement.set('pref.value', true);
      assertTrue(toggleElement.pref!.value);
      assertTrue(toggleElement.checked);
    });

    test('checked property changes on click', () => {
      assertFalse(toggleElement.checked);

      internalToggleElement.click();
      assertTrue(toggleElement.checked);

      internalToggleElement.click();
      assertFalse(toggleElement.checked);
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

      internalToggleElement.click();
      assertTrue(toggleElement.checked);

      const event = await prefChangeEventPromise;
      assertEquals(fakeTogglePref.key, event.detail.prefKey);
      assertEquals(toggleElement.checked, event.detail.value);
    });

    test('Toggling does not update the pref value directly', async () => {
      const initialPrefValue = toggleElement.pref!.value;

      const prefChangeEventPromise =
          eventToPromise('user-action-setting-pref-change', window);
      internalToggleElement.click();
      assertTrue(toggleElement.checked);
      await prefChangeEventPromise;

      // Local pref object should be treated as immutable data and should not be
      // updated directly.
      assertEquals(initialPrefValue, toggleElement.pref!.value);
    });

    test(
        'Pref value updates the checked value when the toggle is disabled',
        () => {
          toggleElement.disabled = true;
          toggleElement.set('pref.value', true);
          assertTrue(toggleElement.pref!.value);
          assertTrue(toggleElement.checked);
        });

    test('toggle is disabled when policy is enforced by pref', async () => {
      await setEnforcedPref();

      assertTrue(toggleElement.isPrefEnforced);
      assertTrue(toggleElement.disabled);
    });

    suite('policy indicator', () => {
      test('is not visible if there is no enforced pref', () => {
        const policyIndicator =
            toggleElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
        assertFalse(isVisible(policyIndicator));
      });

      test('is visible when policy is enforced by pref', async () => {
        await setEnforcedPref();

        const policyIndicator =
            toggleElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);
        assertTrue(isVisible(policyIndicator));

        // policy indicator is still visible even when the toggle is manually
        // changed to enabled.
        toggleElement.disabled = false;
        assertTrue(!!policyIndicator);
      });
    });

    suite('with noSetPref', () => {
      setup(() => {
        toggleElement.noSetPref = true;
      });

      test('toggling does not dispatch a pref change event', async () => {
        const eventPromise =
            eventToPromise('user-action-setting-pref-change', window)
                .then(() => {
                  assertNotReached();
                });

        const successPromise = new Promise((resolve) => {
          setTimeout(() => resolve('success'), 500);
        });

        assertFalse(toggleElement.checked);
        assertFalse(toggleElement.pref!.value);
        internalToggleElement.click();
        assertTrue(toggleElement.checked);

        // eventPromise should never resolve, else it will fail this test.
        const value = await Promise.race([eventPromise, successPromise]);
        assertEquals('success', value);
        assertFalse(toggleElement.pref!.value);
      });

      test(
          'commitPrefChange() updates the pref value to the checked value',
          async () => {
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            internalToggleElement.click();
            assertTrue(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            const prefChangeEventPromise =
                eventToPromise('user-action-setting-pref-change', window);
            toggleElement.commitPrefChange();
            assertTrue(toggleElement.checked);

            const event = await prefChangeEventPromise;
            assertEquals(fakeTogglePref.key, event.detail.prefKey);
            assertEquals(toggleElement.checked, event.detail.value);
          });

      test(
          'resetToPrefValue() changes the checked property to the pref value',
          () => {
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            toggleElement.checked = true;
            assertTrue(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);

            assertTrue(internalToggleElement.checked);

            toggleElement.resetToPrefValue();
            assertFalse(toggleElement.checked);
            assertFalse(toggleElement.pref!.value);
          });
    });

    suite('with inverted enabled', () => {
      setup(async () => {
        await initWithPref(/*prefValue=*/ false, /*isInverted=*/ true);
      });

      test('toggle value is the opposite of the pref', () => {
        assertFalse(toggleElement.pref!.value);
        assertTrue(toggleElement.checked);
      });

      test(
          'clicking on the toggle changes the pref value to the opposite of the toggle',
          async () => {
            const prefChangeEventPromise =
                eventToPromise('user-action-setting-pref-change', window);

            internalToggleElement.click();
            await flushTasks();
            assertFalse(toggleElement.checked);

            const event = await prefChangeEventPromise;
            assertEquals(fakeTogglePref.key, event.detail.prefKey);
            assertEquals(!toggleElement.checked, event.detail.value);
          });

      test(
          'checked value reflects the opposite of pref value when pref changes',
          () => {
            assertFalse(toggleElement.pref!.value);
            assertTrue(toggleElement.checked);

            toggleElement.set('pref.value', true);
            assertTrue(toggleElement.pref!.value);
            assertFalse(toggleElement.checked);
          });
    });
  });

  suite('without pref object', () => {
    setup(async () => {
      await init();

      // there is no pref set.
      assertEquals(toggleElement.pref, undefined);
    });

    test('value changes on click', () => {
      assertFalse(toggleElement.checked);

      internalToggleElement.click();
      assertTrue(toggleElement.checked);

      internalToggleElement.click();
      assertFalse(toggleElement.checked);
    });

    test('control value changes on toggle element change', () => {
      assertFalse(toggleElement.checked);

      toggleElement.checked = true;
      assertTrue(toggleElement.checked);
      assertTrue(internalToggleElement.checked);

      toggleElement.checked = false;
      assertFalse(toggleElement.checked);
      assertFalse(internalToggleElement.checked);
    });

    test('commitPrefChange has no effect when there is no pref', () => {
      assertFalse(toggleElement.checked);

      assertThrows(() => {
        toggleElement.commitPrefChange();
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
