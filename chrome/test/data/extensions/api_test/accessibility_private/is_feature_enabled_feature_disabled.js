// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFeatureDisabled() {
    chrome.accessibilityPrivate.isFeatureEnabled(
        'selectToSpeakNavigationControl', (enabled) => {
          if (!enabled) {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        });
  },
  function testFeatureUnknown() {
    try {
      chrome.accessibilityPrivate.isFeatureEnabled('fooBar', () => {});
      // Should throw error before this point.
      chrome.test.fail();
    } catch (err) {
      // Expect call to throw error.
      chrome.test.succeed();
    }
  }
];

chrome.test.runTests(allTests);
