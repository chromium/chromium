// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test for extension controlled prefs where the underlying
// feature lives in ash. These tests make use of the crosapi to set the value
// in ash. Thus, they run as lacros_chrome_browsertests.
// Run with lacros_chrome_browsertests_run_in_series \
//     --gtest_filter=ExtensionPreferenceLacrosBrowserTest.Lacros
// Based on the "standard" extension test.

// The collection of preferences to test, split into objects with a "root"
// (the root object they preferences are exposed on) and a dictionary of
// preference name -> default value.
var preferencesToTest = [
  {
    root: chrome.accessibilityFeatures,
    preferences: {
      autoclick: false,
    }
  },
  {
    root: chrome.privacy.websites,
    preferences: {
      protectedContentEnabled: true,
    }
  }
];

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

// Verifies that the preference has the expected default value.
function expectDefault(prefName, defaultValue) {
  return expect({
    value: defaultValue,
    levelOfControl: 'controllable_by_this_extension'
  }, '`' + prefName + '` is expected to be the default, which is ' +
     defaultValue);
}

// Verifies that the preference is properly controlled by the extension.
function expectControlled(prefName, newValue) {
  return expect({
    value: newValue,
    levelOfControl: 'controlled_by_this_extension',
  }, '`' + prefName + '` is expected to be controlled by this extension.');
}

// Tests getting the preference value (which should be uncontrolled and at its
// default value).
function prefGetter(prefName, defaultValue) {
  this[prefName].get({}, expectDefault(prefName, defaultValue));
}

// Tests setting the preference value (to the inverse of the default, so that
// it should be controlled by this extension).
function prefSetter(prefName, defaultValue) {
  this[prefName].set({value: !defaultValue},
                     chrome.test.callbackPass(function() {
    this[prefName].get({}, expectControlled(prefName, !defaultValue));
  }.bind(this)));
}

// Tests clearing the preference value (to the inverse of the default, so that
// it should be the default value again).
function prefClearer(prefName, defaultValue) {
  this[prefName].clear({}, chrome.test.callbackPass(function() {
    this[prefName].get({}, expectDefault(prefName, defaultValue));
  }.bind(this)));
}

chrome.test.sendMessage('ready', function(message) {
  if (message != 'run test')
    return;

  chrome.test.runTests([
    function getPreferences() {
      for (let preferenceSet of preferencesToTest) {
        for (let key in preferenceSet.preferences) {
          prefGetter.call(
              preferenceSet.root, key, preferenceSet.preferences[key]);
        }
      }
    },
    function setGlobals() {
      for (let preferenceSet of preferencesToTest) {
        for (let key in preferenceSet.preferences) {
          prefSetter.call(
              preferenceSet.root, key, preferenceSet.preferences[key]);
        }
      }
    },
    function clearGlobals() {
      for (let preferenceSet of preferencesToTest) {
        for (let key in preferenceSet.preferences) {
          prefClearer.call(
              preferenceSet.root, key, preferenceSet.preferences[key]);
        }
      }
    },
    function resetGlobals() {
      for (let preferenceSet of preferencesToTest) {
        for (let key in preferenceSet.preferences) {
          prefSetter.call(
              preferenceSet.root, key, preferenceSet.preferences[key]);
        }
      }
    },
  ]);
});
