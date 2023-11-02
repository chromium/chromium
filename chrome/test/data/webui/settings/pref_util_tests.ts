// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {prefToString, stringToPrefValue} from 'chrome://settings/settings.js';
import {assertEquals, assertNotReached} from 'chrome://webui-test/chai_assert.js';
// clang-format on

/** @fileoverview Suite of tests for Settings.PrefUtil. */
suite('PrefUtil', function() {
  /**
   * @param fn Function that should throw.
   * @param message Message to log if function does not throw.
   */
  function assertThrows(fn: () => void, message: string) {
    try {
      fn();
      assertNotReached(message);
    } catch (e) {
    }
  }

  // Tests that the given value is converted to the expected value, for a
  // given prefType.
  function expectStringToPrefValue(
      value: string, prefType: chrome.settingsPrivate.PrefType,
      expectedValue: string|boolean|number) {
    const pref: chrome.settingsPrivate.PrefObject = {
      type: prefType,
      key: 'foo',
      value: 0,  // this value is irrelevent to |stringToPrefValue|
    };
    assertEquals(expectedValue, stringToPrefValue(value, pref));
  }

  test('stringToPrefValue', function() {
    expectStringToPrefValue(
        'true', chrome.settingsPrivate.PrefType.BOOLEAN, true);

    expectStringToPrefValue(
        'false', chrome.settingsPrivate.PrefType.BOOLEAN, false);

    expectStringToPrefValue('42', chrome.settingsPrivate.PrefType.NUMBER, 42);

    expectStringToPrefValue(
        'Foo Bar', chrome.settingsPrivate.PrefType.STRING, 'Foo Bar');

    const url = 'http://example.com';
    expectStringToPrefValue(url, chrome.settingsPrivate.PrefType.URL, url);

    assertThrows(function() {
      expectStringToPrefValue(
          '[1, 2]', chrome.settingsPrivate.PrefType.LIST, '');
    }, 'List prefs should not be converted.');

    assertThrows(function() {
      expectStringToPrefValue(
          '{foo: 1}', chrome.settingsPrivate.PrefType.DICTIONARY, '');
    }, 'Dictionary prefs should not be converted.');
  });

  // Tests that the pref value is converted to the expected string, for a
  // given prefType.
  function assertPrefToString(
      prefType: chrome.settingsPrivate.PrefType, prefValue: any,
      expectedValue: string|boolean|number|null) {
    const pref = {
      type: prefType,
      value: prefValue,
      key: 'foo',
    };
    assertEquals(expectedValue, prefToString(pref));
  }

  test('prefToString', function() {
    assertPrefToString(chrome.settingsPrivate.PrefType.BOOLEAN, false, 'false');

    assertPrefToString(chrome.settingsPrivate.PrefType.NUMBER, 42, '42');

    assertPrefToString(
        chrome.settingsPrivate.PrefType.STRING, 'Foo Bar', 'Foo Bar');

    const url = 'http://example.com';
    assertPrefToString(chrome.settingsPrivate.PrefType.URL, url, url);

    assertThrows(function() {
      assertPrefToString(chrome.settingsPrivate.PrefType.LIST, [1, 2], null);
    }, 'List prefs should not be handled.');

    assertThrows(function() {
      assertPrefToString(
          chrome.settingsPrivate.PrefType.DICTIONARY, {foo: 1}, null);
    }, 'Dictionary prefs should not be handled.');
  });
});
