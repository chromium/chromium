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
  }
]);
