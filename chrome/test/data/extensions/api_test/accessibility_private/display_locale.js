// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [function testGetDisplayNameForLocale() {
  // The implementation of getDisplayNameForLocale() is more heavily
  // unittested elsewhere; here, we just need a sanity check to make sure
  // everything is correctly wired up.
  chrome.test.assertEq(
      'English',
      chrome.accessibilityPrivate.getDisplayNameForLocale('en', 'en'));
  chrome.test.assertEq(
      'Cantonese (Hong Kong)',
      chrome.accessibilityPrivate.getDisplayNameForLocale('yue-HK', 'en'));
  chrome.test.succeed();
}];
chrome.test.runTests(allTests);
