// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.privacy.services.safeBrowsingEnabled
// Run with browser_tests --gtest_filter=ExtensionApiTest.SafeBrowsing

function setTrue(callback) {
  chrome.privacy.services.safeBrowsingEnabled.set({ value: true }, function() {
    chrome.test.sendMessage("set to true", callback);
  });
}

function setFalse(callback) {
  chrome.privacy.services.safeBrowsingEnabled.set({ value: false }, function() {
    chrome.test.sendMessage("set to false", callback);
  });
}

function clearPref(callback) {
  chrome.privacy.services.safeBrowsingEnabled.clear({}, function() {
    chrome.test.sendMessage("cleared", callback);
  });
}

// Run the following steps:
// 1. Set the pref to true.
// 2. Clear the pref.
// 3. Set the pref to false.
// 4. Done.
// The callback of each step is calling the next step.
setTrue(clearPref.bind(this, setFalse.bind(this, function() {
  chrome.test.sendMessage("done");
})));
