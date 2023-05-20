// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function getFirstTab() {
  let tabs = await chrome.tabs.query({});
  chrome.test.assertTrue(tabs.length >= 1);
  return tabs[0];
}

async function getFirstTabId() {
  return (await getFirstTab()).id;
}

// Test various error cases for the sidePanel.open() API.
// Success cases are tested predominantly in
// chrome/browser/ui/views/side_panel/extensions/extension_side_panel_browsertest.cc.
chrome.test.runTests([
  async function openRequiresUserGesture() {
    const tabId = await getFirstTabId();
    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.open({tabId}),
        'Error: `sidePanel.open()` may only be called in response to a ' +
        'user gesture.');
    chrome.test.succeed();
  },

  async function cannotCallOpenForATabWithNoPanelSet() {
    const tabId = await getFirstTabId();
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

  async function cannotCallOpenForAWindowWithNoPanelSet() {
    const windowId = (await getFirstTab()).windowId;
    // Disable the panel globally.
    await chrome.sidePanel.setOptions({enabled: false});
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({windowId}),
          `Error: No active side panel for windowId: ${windowId}`);
      // Clean up: re-enable the side panel.
      await chrome.sidePanel.setOptions({enabled: true});
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

  async function cannotCallForAnInvalidWindowId() {
    const fakeWindowId = 9999999;
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({windowId: fakeWindowId}),
          `Error: No window with id: ${fakeWindowId}.`);
      chrome.test.succeed();
    });
  },

  async function atLeastOneOfTabIdAndWindowIdNeeded() {
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({}),
          'Error: At least one of `tabId` and `windowId` must be provided');
      chrome.test.succeed();
    });
  },

  async function tabIdAndWindowIdMismatchThrowsError() {
    const firstTabId = await getFirstTabId();
    const newWindow = await chrome.windows.create({url: 'about:blank'});
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.open({tabId: firstTabId, windowId: newWindow.id}),
          'Error: The specified tab does not belong to the specified window.');
      chrome.test.succeed();
    });
  },
]);
