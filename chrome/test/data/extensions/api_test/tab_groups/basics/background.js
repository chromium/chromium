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
    let onCreatedPromise = new Promise((resolve) => {
      chrome.tabGroups.onCreated.addListener((group) => {
        resolve(group.id);
      });
    });

    let createPromise = new Promise((resolve) => {
      chrome.tabs.create({}, (tab) => {
        chrome.tabs.group({tabIds: tab.id}, (groupId) => {
          resolve(groupId);
        });
      });
    });

    Promise.allSettled([onCreatedPromise, createPromise]).then((results) => {
      chrome.test.assertEq(results.length, 2);
      chrome.test.assertEq(results[0], results[1]);
      chrome.test.succeed();
    });
  }
]);
