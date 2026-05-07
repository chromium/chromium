// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function moveGroup() {
    // Create other tabs in window 1 to add to a group.
    await chrome.tabs.create({url: 'about:blank'});
    await chrome.tabs.create({url: 'about:blank'});

    // Get the tabs in window 1. There should now be three of them.
    const tabs = await chrome.tabs.query({currentWindow: true});
    chrome.test.assertEq(3, tabs.length);

    // Create a tab group in window 1 with two tabs.
    const groupId = await chrome.tabs.group({tabIds: [tabs[1].id, tabs[2].id]});

    // Create a new window.
    const window2 = await chrome.windows.create({url: 'about:blank'});

    // Move the tab group to window 2 and wait for it to process.
    await chrome.tabGroups.move(groupId, {windowId: window2.id, index: 0});

    // Two tabs moved, so window 2 now has a total of 3 tabs.
    const tabs2 = await chrome.tabs.query({windowId: window2.id});
    chrome.test.assertEq(3, tabs2.length);

    chrome.test.succeed();
  },
]);
