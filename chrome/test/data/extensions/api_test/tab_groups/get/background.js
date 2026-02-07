// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testGetSucceeds() {
    chrome.tabs.create({}, (tab) => {
      chrome.tabs.group({ tabIds: tab.id }, (groupId) => {
        chrome.tabGroups.get(groupId, (tabGroup) => {
          chrome.test.assertNoLastError();
          chrome.test.assertNe(0, tabGroup.id);
          chrome.test.succeed();
        });
      });
    });
  },
  function testGetFails() {
    // Try to get a non-existent group.
    chrome.tabGroups.get(123, (tabGroup) => {
      chrome.test.assertLastError('No group with id: 123.');
      chrome.test.assertEq(null, tabGroup);
      chrome.test.succeed();
    });
  },
  async function testGetFailsAfterUngroup() {
    // Create a group and then ungroup it, then try to get the group.
    const tab = await chrome.tabs.create({});
    const groupId = await chrome.tabs.group({tabIds: tab.id});
    let tabGroup = await chrome.tabGroups.get(groupId);
    chrome.test.assertNoLastError();
    chrome.test.assertNe(0, tabGroup.id);

    await chrome.tabs.ungroup(tab.id);
    chrome.tabGroups.get(groupId, (tabGroup) => {
      chrome.test.assertLastError(`No group with id: ${groupId}.`);
      chrome.test.assertEq(null, tabGroup);
      chrome.test.succeed();
    });
  }
]);
