// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, PrefControlMixinInternal} from 'chrome://os-settings/os_settings.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../../utils.js';

const TestElementBase = PrefControlMixinInternal(PolymerElement);
class TestElement extends TestElementBase {}
customElements.define('test-element', TestElement);

suite('PrefControlMixinInternal', () => {
  let testElement: TestElement;

  const fakePrefObject = {
    key: 'foo.bar.baz',
    type: chrome.settingsPrivate.PrefType.STRING,
    value: 'initialValue',
  };

  setup(async () => {
    clearBody();
    testElement = document.createElement('test-element') as TestElement;
    testElement.id = 'exampleID';
    document.body.appendChild(testElement);
    await flushTasks();
  });

  teardown(() => {
    CrSettingsPrefs.resetForTesting();
  });

  suite('isPrefEnforced property', () => {
    test('is false if no pref provided', () => {
      assertFalse(testElement.isPrefEnforced);
    });

    test('is false if pref is provided but not enforced', () => {
      testElement.pref = {...fakePrefObject};
      assertFalse(testElement.isPrefEnforced);
    });

    [{
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      isPrefEnforced: true,
    },
     {
       enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
       isPrefEnforced: false,
     },
     {
       enforcement: chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED,
       isPrefEnforced: false,
     },
    ].forEach(({enforcement, isPrefEnforced}) => {
      test(`is ${isPrefEnforced} for pref enforcement: ${enforcement}`, () => {
        testElement.pref = {...fakePrefObject, enforcement};
        assertEquals(isPrefEnforced, testElement.isPrefEnforced);
      });
    });
  });

  suite('disabled property', () => {
    test('should reflect to attribute', () => {
      // `disabled` is false by default.
      assertFalse(testElement.hasAttribute('disabled'));

      testElement.disabled = true;
      assertTrue(testElement.hasAttribute('disabled'));

      testElement.disabled = false;
      assertFalse(testElement.hasAttribute('disabled'));
    });

    test('considers pref enforcement', () => {
      testElement.pref = {...fakePrefObject};
      assertFalse(testElement.disabled);

      testElement.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(testElement.disabled);
    });

    test('cannot be overridden if pref is enforced', () => {
      testElement.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      assertTrue(testElement.disabled);

      // Attempt to force enable the element. Element should still be disabled
      // since the pref is enforced.
      testElement.disabled = false;
      assertTrue(testElement.disabled);
    });
  });

  suite('validatePref()', () => {
    test('pref is not a string', () => {
      // Simulate a string value instead of a PrefObject. Use Object.assign() to
      // bypass typechecking here since there is no typechecking in Polymer HTML
      // data-binding.
      Object.assign(testElement, {pref: 'foobar'});

      assertThrows(() => {
        testElement.validatePref();
      }, 'TEST-ELEMENT#exampleID error: Invalid string literal.');
    });

    test('pref type is invalidated', () => {
      testElement.validPrefTypes = [chrome.settingsPrivate.PrefType.NUMBER];
      testElement.pref = {...fakePrefObject};

      assertThrows(() => {
        testElement.validatePref();
      }, 'TEST-ELEMENT#exampleID error: Invalid pref type STRING.');
    });

    test('pref type is validated', () => {
      testElement.validPrefTypes = [chrome.settingsPrivate.PrefType.STRING];
      testElement.pref = {...fakePrefObject};

      testElement.validatePref();
    });

    test('called once prefs are initialized', async () => {
      let validatePrefCalled = false;
      testElement['validatePref'] = () => {
        validatePrefCalled = true;
      };

      CrSettingsPrefs.setInitialized();
      await flushTasks();
      assertTrue(validatePrefCalled);
    });
  });

  suite('updatePrefValueFromUserAction()', () => {
    test('Raises an error if called when pref is not defined', async () => {
      assertThrows(() => {
        testElement.updatePrefValueFromUserAction('newValue9001');
      }, 'updatePrefValueFromUserAction() requires pref to be defined.');
    });

    test('Dispatches a "user-action-setting-pref-change" event', async () => {
      testElement.pref = {...fakePrefObject};

      const expectedValue = 'newValue9001';
      const eventPromise =
          eventToPromise('user-action-setting-pref-change', window);
      testElement.updatePrefValueFromUserAction(expectedValue);

      const event = await eventPromise;
      assertEquals(fakePrefObject.key, event.detail.prefKey);
      assertEquals(expectedValue, event.detail.value);
    });

    test('Does not update the pref object value directly', async () => {
      testElement.pref = {...fakePrefObject};
      const initialValue = testElement.pref.value;

      const eventPromise =
          eventToPromise('user-action-setting-pref-change', window);
      testElement.updatePrefValueFromUserAction('newValue9001');
      await eventPromise;
      assertEquals(initialValue, testElement.pref.value);
    });
  });
});
