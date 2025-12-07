// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function getFirstTab() {
  let tabs = await chrome.tabs.query({});
  chrome.test.assertTrue(tabs.length >= 1);
  return tabs[0];
}

async function getFirstTabId() {
  const tab = await getFirstTab();
  return tab.id;
}

async function getOnlyWindow() {
  const windows = await chrome.windows.getAll();
  // The browser test should only have a single window.
  chrome.test.assertTrue(windows.length === 1);
  return windows[0];
}

async function getOnlyWindowId() {
  const window = await getOnlyWindow();
  return window.id;
}

// Test various error cases for the sidePanel.close() API.
// Success cases are tested predominantly in
// `chrome/browser/ui/views/side_panel/extensions/
// extension_side_panel_browsertest.cc`.
chrome.test.runTests([
  async function closeWithoutTabOrWindowId() {
    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.close({}),
        'Error: At least one of `tabId` and `windowId` must be provided');
    chrome.test.succeed();
  },

  async function closeWithInvalidTabId() {
    const invalidTabId = 999999;
    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.close({tabId: invalidTabId}),
        `Error: No tab with id: ${invalidTabId}.`);
    chrome.test.succeed();
  },

  async function closeWithInvalidWindowId() {
    const invalidWindowId = 9999999;
    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.close({windowId: invalidWindowId}),
        `Error: No window with id: ${invalidWindowId}.`);
    chrome.test.succeed();
  },

  async function closeForTabWithNoActivePanel() {
    const tabId = await getFirstTabId();
    // Disable the side panel for this tab.
    await chrome.sidePanel.setOptions({tabId, enabled: false});

    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.close({tabId}),
        `Error: No active side panel for tabId: ${tabId}`);

    // Re-enable for cleanup.
    await chrome.sidePanel.setOptions({tabId, enabled: true});
    chrome.test.succeed();
  },

  async function closeForWindowWithNoActivePanel() {
    const windowId = await getOnlyWindowId();
    // Disable the global side panel.
    await chrome.sidePanel.setOptions({enabled: false});

    await chrome.test.assertPromiseRejects(
        chrome.sidePanel.close({windowId}),
        `Error: No active side panel for windowId: ${windowId}`);

    // Re-enable for cleanup.
    await chrome.sidePanel.setOptions({enabled: true});
    chrome.test.succeed();
  },

  async function tabIdAndWindowIdMismatchThrowsError() {
    const firstTabId = await getFirstTabId();
    const newWindow = await chrome.windows.create({url: 'about:blank'});
    chrome.test.runWithUserGesture(async () => {
      await chrome.test.assertPromiseRejects(
          chrome.sidePanel.close({tabId: firstTabId, windowId: newWindow.id}),
          'Error: The specified tab does not belong to the specified window.');
      chrome.test.succeed();
    });
  },

]);
