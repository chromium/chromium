// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {CrSettingsPrefs} from 'chrome://settings/settings.js';
// #import {FakeSettingsPrivate} from 'chrome://test/settings/fake_settings_private.m.js';
// #import {prefsTestCases} from 'chrome://test/settings/prefs_test_cases.m.js';
// clang-format on

/** @fileoverview Suite of tests for settings-prefs. */
cr.define('settings_prefs', function() {
  /**
   * Creates a deep copy of the object.
   * @param {!Object} obj
   * @return {!Object}
   */
  function deepCopy(obj) {
    return JSON.parse(JSON.stringify(obj));
  }

  suite('CrSettingsPrefs', function() {
    /**
     * Prefs instance created before each test.
     * @type {SettingsPrefsElement|undefined}
     */
    let prefs;

    /** @type {settings.FakeSettingsPrivate} */
    let fakeApi = null;

    /**
     * @param {!Object} prefStore Pref store from <settings-prefs>.
     * @param {string} key Pref key of the pref to return.
     * @return {chrome.settingsPrivate.PrefObject|undefined}
     */
    function getPrefFromKey(prefStore, key) {
      const path = key.split('.');
      let pref = prefStore;
      for (const part of path) {
        pref = pref[part];
        if (!pref) {
          return undefined;
        }
      }
      return pref;
    }

    /**
     * Checks that the fake API pref store contains the expected values.
     * @param {number} testCaseValueIndex The index of possible next values
     *     from the test case to check.
     */
    function assertFakeApiPrefsSet(testCaseValueIndex) {
      for (const testCase of prefsTestCases) {
        const expectedValue =
            JSON.stringify(testCase.nextValues[testCaseValueIndex]);
        const actualValue =
            JSON.stringify(fakeApi.prefs[testCase.pref.key].value);
        assertEquals(expectedValue, actualValue, testCase.pref.key);
      }
    }

    /**
     * Checks that the <settings-prefs> contains the expected values.
     * @param {number} testCaseValueIndex The index of possible next values
     *     from the test case to check.
     */
    function assertPrefsSet(testCaseValueIndex) {
      for (const testCase of prefsTestCases) {
        const expectedValue =
            JSON.stringify(testCase.nextValues[testCaseValueIndex]);
        const actualValue =
            JSON.stringify(prefs.get('prefs.' + testCase.pref.key + '.value'));
        assertEquals(expectedValue, actualValue);
      }
    }

    // Initialize a <settings-prefs> before each test.
    setup(function() {
      PolymerTest.clearBody();

      // Override chrome.settingsPrivate with FakeSettingsPrivate.
      fakeApi = new settings.FakeSettingsPrivate(
          prefsTestCases.map(function(testCase) {
            return testCase.pref;
          }));
      CrSettingsPrefs.deferInitialization = true;

      prefs = document.createElement('settings-prefs');
      document.body.appendChild(prefs);
      prefs.initialize(fakeApi);

      // getAllPrefs is asynchronous, so return the prefs promise.
      return CrSettingsPrefs.initialized;
    });

    teardown(function() {
      CrSettingsPrefs.resetForTesting();
      CrSettingsPrefs.deferInitialization = false;
      prefs.resetForTesting();

      PolymerTest.clearBody();
    });

    test('receives and caches prefs', function testGetPrefs() {
      // Test that each pref has been successfully copied to the Polymer
      // |prefs| property.
      for (const key in fakeApi.prefs) {
        const expectedPref = fakeApi.prefs[key];
        const actualPref = getPrefFromKey(prefs.prefs, key);
        if (!expectNotEquals(undefined, actualPref)) {
          // We've already registered an error, so skip the pref.
          continue;
        }

        assertEquals(JSON.stringify(expectedPref), JSON.stringify(actualPref));
      }
    });

    test('forwards pref changes to API', function testSetPrefs() {
      // Test that settings-prefs uses the setPref API.
      for (const testCase of prefsTestCases) {
        prefs.set(
            'prefs.' + testCase.pref.key + '.value',
            deepCopy(testCase.nextValues[0]));
      }
      // Check that setPref has been called for the right values.
      assertFakeApiPrefsSet(0);

      // Test that when setPref fails, the pref is reverted locally.
      for (const testCase of prefsTestCases) {
        fakeApi.failNextSetPref();
        prefs.set(
            'prefs.' + testCase.pref.key + '.value',
            deepCopy(testCase.nextValues[1]));
      }

      assertPrefsSet(0);

      // Test that setPref is not called when the pref doesn't change.
      fakeApi.disallowSetPref();
      for (const testCase of prefsTestCases) {
        prefs.set(
            'prefs.' + testCase.pref.key + '.value',
            deepCopy(testCase.nextValues[0]));
      }
      assertFakeApiPrefsSet(0);
      fakeApi.allowSetPref();
    });

    test('responds to API changes', function testOnChange() {
      // Changes from the API should not result in those changes being sent
      // back to the API, as this could trigger a race condition.
      fakeApi.disallowSetPref();
      let prefChanges = [];
      for (const testCase of prefsTestCases) {
        prefChanges.push(
            {key: testCase.pref.key, value: testCase.nextValues[0]});
      }

      // Send a set of changes.
      fakeApi.sendPrefChanges(prefChanges);
      assertPrefsSet(0);

      prefChanges = [];
      for (const testCase of prefsTestCases) {
        prefChanges.push(
            {key: testCase.pref.key, value: testCase.nextValues[1]});
      }

      // Send a second set of changes.
      fakeApi.sendPrefChanges(prefChanges);
      assertPrefsSet(1);

      // Send the same set of changes again -- nothing should happen.
      fakeApi.sendPrefChanges(prefChanges);
      assertPrefsSet(1);
    });
  });

  // #cr_define_end
});
