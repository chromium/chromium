// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic browser tests for the tabGroups API. Most API behavior is tested in
// tab_groups_api_unittest.cc, this just provides end-to-end coverage.
chrome.test.runTests([
  function testQuerySucceeds() {
    chrome.tabs.create({}, (tab) => {
      chrome.tabs.group({tabIds: tab.id}, (groupId) => {
        chrome.tabGroups.query({windowId: -2}, (groupList) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(1, groupList.length);
          chrome.test.succeed();
        });
      });
    });
  },
  function testUpdateSucceeds() {
    chrome.tabs.create({}, (tab) => {
      chrome.tabs.group({tabIds: tab.id}, (groupId) => {
        chrome.tabGroups.update(groupId, {title: 'Title'}, (group) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Title', group.title);
          chrome.test.succeed();
        });
      });
    });
  },
  function testCreateEventDispatched() {
    let createdGroupId = -1;

    chrome.tabGroups.onCreated.addListener((group) => {
      chrome.test.assertEq(group.id, createdGroupId);
      chrome.test.succeed();
    });

    chrome.tabs.create({}, (tab) => {
      chrome.tabs.group({tabIds: tab.id}, (groupId) => {
        createdGroupId = groupId;
      });
    });
  }
]);
