// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Preferences API test for extension controlled prefs where the underlying
// prefs should live in Ash but are not supported in the current Ash version due
// to version skew. These tests make use of the crosapi to set the value in Ash.
// Thus, they run as lacros_chrome_browsertests.
// Run with lacros_chrome_browsertests \
//     --gtest_filter=*/ExtensionPreferenceApiUnsupportedInAshBrowserTest.*/*
// Based on the "standard" extension test.
chrome.test.runTests([
  function getPreference() {
    chrome.accessibilityFeatures.autoclick.get({}, (value) => {
      chrome.test.assertEq(null, value);
      chrome.test.assertEq(
          chrome.runtime.lastError.message,
          'The browser preference is not supported.');
      chrome.test.succeed();
    });
  },
  function setPreference() {
    chrome.accessibilityFeatures.autoclick.set({value: true}, () => {
      chrome.test.succeed();
    });
  },
  function clearPreference() {
    chrome.accessibilityFeatures.autoclick.clear({}, () => {
      chrome.test.succeed();
    });
  },
]);
