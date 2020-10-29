// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFeatureEnabled() {
    chrome.accessibilityPrivate.isFeatureEnabled(
        'selectToSpeakNavigationControl', (enabled) => {
          if (enabled) {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        });
  },
];

chrome.test.runTests(allTests);
