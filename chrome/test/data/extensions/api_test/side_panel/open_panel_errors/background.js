// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function getFirstTab() {
  let tabs = await chrome.tabs.query({});
  chrome.test.assertTrue(tabs.length >= 1);
  return tabs[0].id;
}

// Test various error cases for the sidePanel.open() API.
// Success cases are tested predominantly in
// chrome/browser/ui/views/side_panel/extensions/extension_side_panel_browsertest.cc.
chrome.test.runTests([
  async function openRequiresUserGesture() {
    const tabId = await getFirstTab();
    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.open({tabId}),
        'Error: `sidePanel.open()` may only be called in response to a ' +
        'user gesture.');
    chrome.test.succeed();
  },

  async function cannotCallOpenForATabWithNoPanelSet() {
    const tabId = await getFirstTab();
    await chrome.sidePanel.setOptions({tabId, enabled: false});
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({tabId}),
          `Error: No active side panel for tabId: ${tabId}`);
      // Clean up: re-enable the side panel on the tab.
      await chrome.sidePanel.setOptions({tabId, enabled: true});
      chrome.test.succeed();
    });
  },

  async function cannotCallOpenForAnInvalidTabId() {
    const fakeTabId = 9999999;
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({tabId: fakeTabId}),
          `Error: No tab with tabId: ${fakeTabId}`);
      chrome.test.succeed();
    });
  },
]);
