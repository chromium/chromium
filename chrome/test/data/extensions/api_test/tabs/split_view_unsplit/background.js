// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function createFreshTab(options = {}) {
  return await chrome.tabs.create({url: 'about:blank', ...options});
}

chrome.test.runTests([
  async function validUnsplit() {
    let tab1 = await createFreshTab();
    let tab2 = await chrome.tabs.create({splitWithTabId: tab1.id});
    tab1 = await chrome.tabs.get(tab1.id);
    // Verify that the split view was created successfully.
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, tab1.splitViewId);
    chrome.test.assertEq(tab1.splitViewId, tab2.splitViewId);

    // Unsplit the split view.
    await chrome.tabs.unsplit(tab1.splitViewId);

    // Verify that the split view was unsplit successfully.
    tab1 = await chrome.tabs.get(tab1.id);
    tab2 = await chrome.tabs.get(tab2.id);
    chrome.test.assertEq(chrome.tabs.SPLIT_VIEW_ID_NONE, tab1.splitViewId);
    chrome.test.assertEq(chrome.tabs.SPLIT_VIEW_ID_NONE, tab2.splitViewId);

    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function errorInvalidSplitViewId() {
    await chrome.test.assertPromiseRejects(
        chrome.tabs.unsplit(999999), 'Error: No split view with id: 999999.');
    chrome.test.succeed();
  },

  async function errorAlreadyUnsplit() {
    let tab1 = await createFreshTab();
    const tab2 = await chrome.tabs.create({splitWithTabId: tab1.id});
    tab1 = await chrome.tabs.get(tab1.id);
    // Verify that the split view was created successfully.
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, tab1.splitViewId);
    chrome.test.assertEq(tab1.splitViewId, tab2.splitViewId);

    const splitViewId = tab1.splitViewId;
    await chrome.tabs.unsplit(splitViewId);
    // Split view ID is invalid after the previous unsplit.
    await chrome.test.assertPromiseRejects(
        chrome.tabs.unsplit(splitViewId),
        `Error: No split view with id: ${splitViewId}.`);

    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function validUnsplitInGroup() {
    let groupedTab = await createFreshTab();
    const groupId = await chrome.tabs.group({tabIds: [groupedTab.id]});
    let newTab = await chrome.tabs.create({splitWithTabId: groupedTab.id});

    groupedTab = await chrome.tabs.get(groupedTab.id);
    await chrome.tabs.unsplit(groupedTab.splitViewId);

    // Verify that the tabs remain in the same group but are no longer in a
    // split view.
    groupedTab = await chrome.tabs.get(groupedTab.id);
    newTab = await chrome.tabs.get(newTab.id);
    chrome.test.assertEq(
        chrome.tabs.SPLIT_VIEW_ID_NONE, groupedTab.splitViewId);
    chrome.test.assertEq(chrome.tabs.SPLIT_VIEW_ID_NONE, newTab.splitViewId);
    chrome.test.assertEq(groupId, groupedTab.groupId);
    chrome.test.assertEq(groupId, newTab.groupId);

    await chrome.tabs.remove([groupedTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function validUnsplitInPinned() {
    let pinnedTab = await createFreshTab({pinned: true});
    let newTab = await chrome.tabs.create({splitWithTabId: pinnedTab.id});

    pinnedTab = await chrome.tabs.get(pinnedTab.id);
    await chrome.tabs.unsplit(pinnedTab.splitViewId);

    // Verify that the tabs remain pinned but are no longer in a split view.
    pinnedTab = await chrome.tabs.get(pinnedTab.id);
    newTab = await chrome.tabs.get(newTab.id);
    chrome.test.assertEq(chrome.tabs.SPLIT_VIEW_ID_NONE, pinnedTab.splitViewId);
    chrome.test.assertEq(chrome.tabs.SPLIT_VIEW_ID_NONE, newTab.splitViewId);
    chrome.test.assertTrue(pinnedTab.pinned);
    chrome.test.assertTrue(newTab.pinned);

    await chrome.tabs.remove([pinnedTab.id, newTab.id]);
    chrome.test.succeed();
  },
]);
