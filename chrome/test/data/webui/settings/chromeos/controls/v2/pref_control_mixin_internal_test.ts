// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefControlMixinInternal} from 'chrome://os-settings/os_settings.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {clearBody} from '../../utils.js';

const TestElementBase = PrefControlMixinInternal(PolymerElement);
class TestElement extends TestElementBase {}
customElements.define('test-element', TestElement);

suite('PrefControlMixinInternal', () => {
  let testElement: TestElement;

  const fakePrefObject = {
    key: 'testPref',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };

  setup(() => {
    clearBody();
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
  });

  suite(
      'pref property',
      () => {
          // TODO(b/333454004) Add pref validation tests.
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

    test('is readonly', () => {
      // Attempt to set property.
      testElement.isPrefEnforced = true;

      // Property does not change.
      assertFalse(testElement.isPrefEnforced);
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
  });
});
