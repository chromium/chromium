// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

/** @fileoverview Suite of tests for settings-checkbox. */
suite('SettingsCheckbox', function() {
  /**
   * Checkbox created before each test.
   * @type {SettingsCheckbox}
   */
  let testElement;

  /**
   * Pref value used in tests, should reflect checkbox 'checked' attribute.
   * @type {SettingsCheckbox}
   */
  const pref = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true
  };

  // Initialize a checked settings-checkbox before each test.
  setup(function() {
    PolymerTest.clearBody();
    testElement = document.createElement('settings-checkbox');
    testElement.set('pref', pref);
    document.body.appendChild(testElement);
  });

  test('value changes on click', function() {
    assertTrue(testElement.checked);

    testElement.$.checkbox.click();
    assertFalse(testElement.checked);
    assertFalse(pref.value);

    testElement.$.checkbox.click();
    assertTrue(testElement.checked);
    assertTrue(pref.value);
  });

  test('fires a change event', function(done) {
    testElement.addEventListener('change', function() {
      assertFalse(testElement.checked);
      done();
    });
    testElement.$.checkbox.click();
  });

  test('does not change when disabled', function() {
    testElement.checked = false;
    testElement.setAttribute('disabled', '');
    assertTrue(testElement.disabled);
    assertTrue(testElement.$.checkbox.disabled);

    testElement.$.checkbox.click();
    assertFalse(testElement.checked);
    assertFalse(testElement.$.checkbox.checked);
  });

  test('numerical pref', function() {
    const prefNum = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1
    };

    testElement.set('pref', prefNum);
    assertTrue(testElement.checked);

    testElement.$.checkbox.click();
    assertFalse(testElement.checked);
    assertEquals(0, prefNum.value);

    testElement.$.checkbox.click();
    assertTrue(testElement.checked);
    assertEquals(1, prefNum.value);
  });
});
