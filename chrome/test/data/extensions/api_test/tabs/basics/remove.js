// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var secondTabId;
chrome.test.runTests([
  function createSecondTab() {
    // Create and switch to a second tab that has an unload handler.
    chrome.tabs.create({index: 1, active: true, url: 'unload-storage-1.html'},
      (tab) => {
        secondTabId = tab.id;
        assertTrue(tab.active);
        assertEq(1, tab.index);
        chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
          // Wait for the second tab to finish loading before moving on.
          if (tabId == secondTabId && changeInfo.status == 'complete') {
            chrome.test.succeed();
          }
        });
      });
  },
  function removeSecondTab() {
    chrome.tabs.remove(secondTabId, () => {
      // The second tab should've set the 'did_run_unload_1' value from
      // its unload handler, which is accessible from the first tab too.
      assertEq('yes', localStorage.getItem('did_run_unload_1'));
      chrome.test.succeed();
    });
  }
]);
