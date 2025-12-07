// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

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

function testUpdate() {
  let tabId;
  // Create a tab to update.
  chrome.tabs.create({ "url": pageUrl("a") }, pass(function (tab) {
    tabId = tab.id;
    // Update the tab URL to "B".
    chrome.tabs.update(tabId, { "url": pageUrl("b") }, pass(function (tab) {
      waitForAllTabs(pass(function () {
        chrome.tabs.get(tabId, pass(function (tab) {
          // Tab navigated to "B".
          assertEq(pageUrl("b"), tab.url);
          chrome.test.succeed();
        }));
      }));
    }));
  }));
}

chrome.test.runTests([testCreateTab, testOnUpdated, testUpdate]);
