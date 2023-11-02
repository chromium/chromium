// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// System API test
// Run with browser_tests \
//     --gtest_filter=ExtensionApiTest.GetIncognitoModeAvailability

chrome.test.runTests([
  function getIncognitoModeAvailabilityTest() {
    chrome.systemPrivate.getIncognitoModeAvailability(
        chrome.test.callbackPass(function(value) {
          chrome.test.assertEq('disabled', value);
        }));
  },
]);
