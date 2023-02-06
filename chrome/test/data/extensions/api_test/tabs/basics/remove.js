// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function resolveOnMessage(resolve) {
  chrome.runtime.onMessage.addListener(function local(message) {
    chrome.runtime.onMessage.removeListener(local);
    assertEq('did_run_unload_1', message);
    resolve();
  });
}

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
    let onMessagePromise = new Promise(resolveOnMessage);

    let removePromise = new Promise((resolve) => {
      chrome.tabs.remove(secondTabId, () => {
      resolve();
      });
    });

    Promise.all([onMessagePromise, removePromise]).then(chrome.test.succeed);
  }
]);
