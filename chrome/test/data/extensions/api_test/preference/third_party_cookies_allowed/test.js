// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests
// --gtest_filter=ExtensionPreferenceApiTest.ThirdPartyCookiesAllowed

function setTrue() {
  return new Promise((resolve) => {
    chrome.privacy.websites.thirdPartyCookiesAllowed.set({ value: true },
      () => chrome.test.sendMessage("set to true", resolve))
  });
}

function setFalse() {
  return new Promise((resolve) => {
    chrome.privacy.websites.thirdPartyCookiesAllowed.set({ value: false },
      () => chrome.test.sendMessage("set to false", resolve))
  });
}

function clearPref() {
  return new Promise((resolve) => {
    chrome.privacy.websites.thirdPartyCookiesAllowed.clear({},
      () => chrome.test.sendMessage("cleared", resolve))
  });
}

(async () => {
  await setTrue();
  await clearPref();
  await setFalse();
  chrome.test.sendMessage("done");
})();
