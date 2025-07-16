// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { openTab } from '/_test_resources/test_util/tabs_util.js';

// The following test:
// * Opens two tabs.
// * Adds the tabs to a group
// * Removes one of the two tabs
// * Waits for the corresponding tabs.onUpdated and tabs.onRemoved events.
chrome.test.runTests([
  async function addTabsToGroupAndRemove() {
    let windowId;
    let tab1;
    let tab2;
    let resolveTabGroupUpdated;
    let resolveTabRemoved;
    // Promises to wait for the given events from the tabs.
    let tabGroupUpdatedPromise = new Promise((resolve) => {
      resolveTabGroupUpdated = resolve;
    });
    let tabRemovedPromise = new Promise((resolve) => {
      resolveTabRemoved = resolve;
    });

    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
      if (!changeInfo.groupId || changeInfo.groupId != -1) {
        return;
      }

      // The event we receive is for tab2, since it is no longer in the tab
      // group. The removed tab should match `tab2`'s state...
      chrome.test.assertEq(tab2.id, tabId);
      chrome.test.assertEq(tab2.id, tab.id);
      chrome.test.assertEq(tab2.windowId, tab.windowId);

      // ...*Except* that the index is now -1. Since the tab was removed, it is
      // no longer part of a window.
      chrome.test.assertEq(-1, tab.index);

      // Remove the listener (so we don't enter here again) and resolve the
      // promise.
      chrome.tabs.onUpdated.removeListener(listener);
      resolveTabGroupUpdated();
    });

    chrome.tabs.onRemoved.addListener(
        function listener(tabId, removeInfo) {
      // `tab2` is the removed tab.
      chrome.test.assertEq(tab2.id, tabId);
      chrome.test.assertEq(tab2.windowId, removeInfo.windowId);

      // Remove the listener (so we don't enter here again) and resolve the
      // promise.
      chrome.tabs.onRemoved.removeListener(listener);
      resolveTabRemoved();
    });

    // Create two tabs.
    tab1 = await openTab('chrome://version');
    tab2 = await openTab('chrome://about');
    chrome.test.assertNe(tab1.id, tab2.id);
    chrome.test.assertNe(tab1.index, tab2.index);
    chrome.test.assertEq(tab1.windowId, tab2.windowId);

    // Group them together.
    await chrome.tabs.group(
        {
          createProperties: {windowId: tab1.windowId},
          tabIds: [tab1.id, tab2.id],
        });

    // Remove the second tab.
    await chrome.tabs.remove(tab2.id);

    // Wait for the events, where most of the verification happens.
    await Promise.all([tabGroupUpdatedPromise, tabRemovedPromise]);
    chrome.test.succeed();
  }
]);
