// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test for extension controlled prefs where the underlying
// feature lives in ash. These tests make use of the crosapi to set the value
// in ash. Thus, they run as lacros_chrome_browsertests. This test verifies the
// hehavior of the onChange callback.
// Run with lacros_chrome_browsertests_run_in_series \
//     --gtest_filter=ExtensionPreferenceLacrosBrowserTest.Lacros

// Listen until |event| has fired with the |expected| value.
function listenUntil(event, expected) {
  var done = chrome.test.listenForever(event, function(value) {
    chrome.test.assertEq(expected, value);
    done();
  });
}

var af = chrome.accessibilityFeatures;
chrome.test.runTests([
  function changeDefault() {
    listenUntil(af.autoclick.onChange, {
      value: false,
      levelOfControl: 'controlled_by_this_extension'
    });
    af.autoclick.set({
      value:false
    }, chrome.test.callbackPass());
  },
  function clearDefault() {
    listenUntil(af.autoclick.onChange, {
      value: false,
      levelOfControl: 'controllable_by_this_extension'
    });
    af.autoclick.clear({}, chrome.test.callbackPass());
  }
]);
