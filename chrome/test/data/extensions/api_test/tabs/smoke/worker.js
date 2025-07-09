// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testCreateTab() {
  const tab = await chrome.tabs.create({ url: 'about:blank' });
  chrome.test.assertNoLastError();
  // Check that some tab properties are set. The exact values are not
  // important, just ensure they are valid.
  chrome.test.assertTrue(tab.id != null, 'id not valid');
  chrome.test.assertTrue(tab.windowId != null, 'windowId not valid');
  chrome.test.succeed();
}

async function testOnUpdated() {
  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    // Wait for load complete event.
    if (changeInfo.status !== 'complete') {
      return;
    }
    chrome.test.assertNoLastError();
    // The exact value is not important.
    chrome.test.assertTrue(tabId != null, 'tabId not valid');
    // Clean up for tests that might run after this one.
    chrome.tabs.onUpdated.removeListener(listener);
    chrome.test.succeed();
  });
  // Don't use about:blank because on Android it doesn't cause the WebContents
  // to load any web content. The version page is light-weight enough.
  const tab = await chrome.tabs.create({ url: 'chrome://version' });
  // The exact value is not important.
  chrome.test.assertTrue(tab.id != null, 'id not valid');
  // chrome.test.succeed() will be called in the closure above.
}

chrome.test.runTests([testCreateTab, testOnUpdated]);
