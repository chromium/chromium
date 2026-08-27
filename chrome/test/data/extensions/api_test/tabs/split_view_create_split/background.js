// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function createFreshTab() {
  return await chrome.tabs.create({url: 'about:blank'});
}

chrome.test.runTests([
  async function validCreateSplit() {
    let tab1 = await createFreshTab();
    let tab2 = await createFreshTab();
    const splitViewId = await chrome.tabs.createSplit([tab1.id, tab2.id]);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, splitViewId);
    tab1 = await chrome.tabs.get(tab1.id);
    tab2 = await chrome.tabs.get(tab2.id);
    chrome.test.assertEq(splitViewId, tab1.splitViewId);
    chrome.test.assertEq(splitViewId, tab2.splitViewId);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function validCreateSplitReverseOrder() {
    let tab1 = await createFreshTab();
    let tab2 = await createFreshTab();
    const splitViewId = await chrome.tabs.createSplit([tab2.id, tab1.id]);
    chrome.test.assertNe(chrome.tabs.SPLIT_VIEW_ID_NONE, splitViewId);
    tab1 = await chrome.tabs.get(tab1.id);
    tab2 = await chrome.tabs.get(tab2.id);
    chrome.test.assertEq(splitViewId, tab1.splitViewId);
    chrome.test.assertEq(splitViewId, tab2.splitViewId);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function errorDuplicateTabIds() {
    const tab = await createFreshTab();
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab.id, tab.id]),
        'Error: Cannot create a split view with duplicate tab IDs.');
    await chrome.tabs.remove(tab.id);
    chrome.test.succeed();
  },

  async function errorNonAdjacentTabs() {
    const tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    const tab3 = await createFreshTab();
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab1.id, tab3.id]),
        'Error: Cannot create split view with non-adjacent tabs.');
    await chrome.tabs.remove([tab1.id, tab2.id, tab3.id]);
    chrome.test.succeed();
  },

  async function errorTooFewTabIds() {
    const tab = await createFreshTab();
    chrome.test.assertThrows(
        chrome.tabs.createSplit.bind(null, [tab.id]),
        /Error in invocation of tabs\.createSplit\(array tabIds, optional function callback\): Error at parameter 'tabIds': Array must have at least 2 items; found 1\./);
    await chrome.tabs.remove(tab.id);
    chrome.test.succeed();
  },

  async function errorTooManyTabIds() {
    const tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    const tab3 = await createFreshTab();
    chrome.test.assertThrows(
        chrome.tabs.createSplit.bind(null, [tab1.id, tab2.id, tab3.id]),
        /Error in invocation of tabs\.createSplit\(array tabIds, optional function callback\): Error at parameter 'tabIds': Array must have at most 2 items; found 3\./);
    await chrome.tabs.remove([tab1.id, tab2.id, tab3.id]);
    chrome.test.succeed();
  },

  async function errorNotInSameWindow() {
    const tab1 = await createFreshTab();
    const newWindow = await chrome.windows.create({url: ['about:blank']});
    const newTab = (await chrome.tabs.query({windowId: newWindow.id}))[0];
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab1.id, newTab.id]),
        `Error: Cannot create split view with tabs of mismatching 'windowId' ` +
            `states.`);
    await chrome.tabs.remove(tab1.id);
    await chrome.windows.remove(newWindow.id);
    chrome.test.succeed();
  },

  async function errorAlreadyInSplitView() {
    const tab1 = await createFreshTab();
    const tab2 = await chrome.tabs.create({splitWithTabId: tab1.id});
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab1.id, tab2.id]),
        `Error: Tab ID ${tab1.id} is already in a split view.`);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function errorPinnedMismatch() {
    const tab1 = await createFreshTab();
    const tab2 = await chrome.tabs.create({url: 'about:blank', pinned: true});
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab1.id, tab2.id]),
        `Error: Cannot create split view with tabs of mismatching 'pinned' ` +
            `states.`);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },

  async function errorGroupMismatch() {
    const tab1 = await createFreshTab();
    const tab2 = await createFreshTab();
    await chrome.tabs.group({tabIds: [tab1.id]});
    await chrome.test.assertPromiseRejects(
        chrome.tabs.createSplit([tab1.id, tab2.id]),
        `Error: Cannot create split view with tabs of mismatching 'groupId' ` +
            `states.`);
    await chrome.tabs.remove([tab1.id, tab2.id]);
    chrome.test.succeed();
  },
]);
