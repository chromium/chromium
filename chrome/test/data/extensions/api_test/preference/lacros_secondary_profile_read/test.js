// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test for extension controlled prefs where the underlying
// feature lives in ash. These tests verify that prefs can be read by
// extensions running in Lacros secondary profile.
// Run with lacros_chrome_browsertests_run_in_series \
//     --ash-chrome-path {path_to_ash_build}/test_ash_chrome
//     --gtest_filter= \
//         ExtensionPreferenceLacrosBrowserTest.LacrosSecondaryProfile

// The collection of preferences to test, split into objects with a "root"
// (the root object they preferences are exposed on) and a dictionary of
// preference name -> expected value.
var preferencesToTestDefault = [{
    root: chrome.accessibilityFeatures,
    preferences: {
      spokenFeedback: false,
    }
  }
];

var preferencesToTestChanged = [{
  root: chrome.accessibilityFeatures,
  preferences: {
    spokenFeedback: true,
  }
}, ];

// Verifies that the preference has the expected value.
function expectPrefValue(prefName, expectedValue) {
  return chrome.test.callbackPass(function (result) {
    chrome.test.assertEq(expectedValue, result.value,
      'Unexpected value for pref `' + prefName + '`');
  });
}

// Tests getting the preference value.
function prefGetter(prefName, expectedValue) {
  this[prefName].get({}, expectPrefValue(prefName, expectedValue));
}

function getPreferences(preferencesToTest) {
  for (let preferenceSet of preferencesToTest) {
    for (let key in preferenceSet.preferences) {
      prefGetter.call(
        preferenceSet.root, key, preferenceSet.preferences[key]);
    }
  }
}

chrome.test.sendMessage('ready', function (message) {
  if (message == 'run test default value') {
    chrome.test.runTests([
      function getPreferencesDefault() {
        getPreferences(preferencesToTestDefault);
      },
    ]);
  } else if (message == 'run test changed value') {
    chrome.test.runTests([
      function getPreferencesChanged() {
        getPreferences(preferencesToTestChanged);
      },
    ]);
  }
});
