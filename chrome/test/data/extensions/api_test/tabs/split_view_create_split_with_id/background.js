// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function createFreshTab(options = {}) {
  return await chrome.tabs.create({url: 'about:blank', ...options});
}

chrome.test.runTests([
  async function validUnspecifiedIndex() {
    let firstTab = await createFreshTab();
    const newTab = await chrome.tabs.create({splitWithTabId: firstTab.id});
    firstTab = await chrome.tabs.get(firstTab.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, firstTab.splitViewId);
    chrome.test.assertEq(firstTab.splitViewId, newTab.splitViewId);
    // Default to the right of the target tab.
    chrome.test.assertEq(newTab.index, firstTab.index + 1);
    await chrome.tabs.remove([firstTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function validAdjacentIndexRight() {
    let firstTab = await createFreshTab();
    const newTab = await chrome.tabs.create(
        {splitWithTabId: firstTab.id, index: firstTab.index + 1});
    firstTab = await chrome.tabs.get(firstTab.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, firstTab.splitViewId);
    chrome.test.assertEq(firstTab.splitViewId, newTab.splitViewId);
    chrome.test.assertEq(newTab.index, firstTab.index + 1);
    await chrome.tabs.remove([firstTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function validAdjacentIndexLeft() {
    let firstTab = await createFreshTab();
    const newTab = await chrome.tabs.create(
        {splitWithTabId: firstTab.id, index: firstTab.index});
    firstTab = await chrome.tabs.get(firstTab.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, firstTab.splitViewId);
    chrome.test.assertEq(firstTab.splitViewId, newTab.splitViewId);
    chrome.test.assertEq(newTab.index, firstTab.index - 1);
    await chrome.tabs.remove([firstTab.id, newTab.id]);
    chrome.test.succeed();
  },

  // Checks that when create is called with active:false and the
  // splitWithTab is NOT active, the split view will be created in the
  // background.
  async function validBackgroundSplitActiveFalse() {
    let tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    // Currently tab2 is active. Calling create with active: false on tab1
    // will create the split view in the background without changing the
    // active tab.
    const newTab =
        await chrome.tabs.create({splitWithTabId: tab1.id, active: false});
    tab1 = await chrome.tabs.get(tab1.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, tab1.splitViewId);
    chrome.test.assertEq(tab1.splitViewId, newTab.splitViewId);
    // Check that the active tab is still tab2.
    const activeTab =
        (await chrome.tabs.query({active: true, currentWindow: true}))[0];
    chrome.test.assertEq(activeTab.id, tab2.id);
    await chrome.tabs.remove([tab1.id, tab2.id, newTab.id]);
    chrome.test.succeed();
  },

  // Checks that when create is called with active:true and the splitWithTab
  // is NOT active, the new tab is created as active.
  async function validBackgroundSplitActiveTrue() {
    let tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    const newTab =
        await chrome.tabs.create({splitWithTabId: tab1.id, active: true});

    tab1 = await chrome.tabs.get(tab1.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, tab1.splitViewId);
    chrome.test.assertEq(tab1.splitViewId, newTab.splitViewId);
    // Check that the active tab is the new tab.
    const activeTab =
        (await chrome.tabs.query({active: true, currentWindow: true}))[0];
    chrome.test.assertEq(activeTab.id, newTab.id);
    await chrome.tabs.remove([tab1.id, tab2.id, newTab.id]);
    chrome.test.succeed();
  },

  // Checks that when create is called with active:false and the
  // splitWithTab is active, the browser will keep the splitWithTab as
  // the active tab.
  async function validNewTabActiveFalse() {
    let firstTab = await createFreshTab();
    const newTab =
        await chrome.tabs.create({splitWithTabId: firstTab.id, active: false});
    firstTab = await chrome.tabs.get(firstTab.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, firstTab.splitViewId);
    chrome.test.assertEq(firstTab.splitViewId, newTab.splitViewId);
    const activeTab =
        (await chrome.tabs.query({active: true, currentWindow: true}))[0];
    chrome.test.assertEq(activeTab.id, firstTab.id);
    await chrome.tabs.remove([firstTab.id, newTab.id]);
    chrome.test.succeed();
  },

  // Checks that when create is called with active:true, the new tab is
  // created as active.
  async function validNewTabActiveTrue() {
    let firstTab = await createFreshTab();
    const newTab =
        await chrome.tabs.create({splitWithTabId: firstTab.id, active: true});
    firstTab = await chrome.tabs.get(firstTab.id);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, firstTab.splitViewId);
    chrome.test.assertEq(firstTab.splitViewId, newTab.splitViewId);
    const activeTab =
        (await chrome.tabs.query({active: true, currentWindow: true}))[0];
    chrome.test.assertEq(activeTab.id, newTab.id);
    await chrome.tabs.remove([firstTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function validSplitAtLeftOfPinnedTab() {
    let pinnedTab = await createFreshTab({pinned: true});
    // Insert new tab to the left of the pinned tab.
    const newTab = await chrome.tabs.create(
        {splitWithTabId: pinnedTab.id, index: pinnedTab.index});
    pinnedTab = await chrome.tabs.get(pinnedTab.id);
    chrome.test.assertTrue(pinnedTab.pinned);
    chrome.test.assertTrue(newTab.pinned);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, pinnedTab.splitViewId);
    chrome.test.assertEq(pinnedTab.splitViewId, newTab.splitViewId);
    chrome.test.assertEq(newTab.index, pinnedTab.index - 1);
    await chrome.tabs.remove([pinnedTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function validSplitAtLeftOfGroupedTab() {
    let groupedTab = await createFreshTab();
    const groupId = await chrome.tabs.group({tabIds: [groupedTab.id]});
    // Insert new tab to the left of the grouped tab.
    const newTab = await chrome.tabs.create(
        {splitWithTabId: groupedTab.id, index: groupedTab.index});
    groupedTab = await chrome.tabs.get(groupedTab.id);
    chrome.test.assertEq(groupedTab.groupId, groupId);
    chrome.test.assertEq(groupedTab.groupId, newTab.groupId);
    chrome.test.assertNe(
        chrome.tabs.SPLIT_VIEW_ID_NONE, groupedTab.splitViewId);
    chrome.test.assertEq(groupedTab.splitViewId, newTab.splitViewId);
    chrome.test.assertEq(newTab.index, groupedTab.index - 1);
    await chrome.tabs.remove([groupedTab.id, newTab.id]);
    chrome.test.succeed();
  },

  async function errorTabNotFound() {
    await chrome.test.assertPromiseRejects(
        chrome.tabs.create({splitWithTabId: 999999}),
        'Error: No tab with id: 999999.');
    chrome.test.succeed();
  },

  async function errorAlreadyInSplitView() {
    const tab = await createFreshTab();
    const firstSplit = await chrome.tabs.create({splitWithTabId: tab.id});
    await chrome.test.assertPromiseRejects(
        chrome.tabs.create({splitWithTabId: tab.id}),
        `Error: Cannot create split view with 'splitWithTabId': ${tab.id}. ` +
            `Tab is already in a split view.`);
    await chrome.tabs.remove([tab.id, firstSplit.id]);
    chrome.test.succeed();
  },

  async function errorNotInSameWindow() {
    const primaryWin = (await chrome.windows.getAll())[0];
    const tab =
        await chrome.tabs.create({windowId: primaryWin.id, url: 'about:blank'});
    const otherWin = await chrome.windows.create({url: ['about:blank']});
    await chrome.test.assertPromiseRejects(
        chrome.tabs.create({
          windowId: otherWin.id,
          splitWithTabId: tab.id,
        }),
        `Error: Cannot create split view with 'splitWithTabId': ${tab.id}. ` +
            `Tab is not in the same window as the target window.`);
    await chrome.tabs.remove(tab.id);
    await chrome.windows.remove(otherWin.id);
    await chrome.windows.update(primaryWin.id, {focused: true});
    chrome.test.succeed();
  },

  async function errorIndexTooSmall() {
    const tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    await chrome.test.assertPromiseRejects(
        chrome.tabs.create({splitWithTabId: tab2.id, index: 0}),
        `Error: Cannot create split view with 'splitWithTabId': Tab ID ` +
            `${tab2.id} is at index ${tab2.index}, which is not adjacent to ` +
            `'index' 0.`);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function errorIndexTooLarge() {
    const tab = await createFreshTab();
    await chrome.test.assertPromiseRejects(
        chrome.tabs.create({splitWithTabId: tab.id, index: 100}),
        `Error: Cannot create split view with 'splitWithTabId': Tab ID ` +
            `${tab.id} is at index ${tab.index}, which is not adjacent to ` +
            `'index' 100.`);
    await chrome.tabs.remove(tab.id);
    chrome.test.succeed();
  },
]);
