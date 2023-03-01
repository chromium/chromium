// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function resolveOnStorageChanged(resolve) {
  chrome.storage.local.onChanged.addListener(function local(changes,
                                                            areaName) {
    assertEq({'newValue': 'yes'}, changes['did_run_unload_1'])
    chrome.storage.local.onChanged.removeListener(local);
    resolve();
  });
}

var secondTabId;
const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function createSecondTab() {
    // Create and switch to a second tab that has an unload handler.
    chrome.tabs.create({index: 1, active: true,
                        url: pageUrl('unload-storage-1')},
      (tab) => {
        secondTabId = tab.id;
        assertTrue(tab.active);
        assertEq(1, tab.index);
        chrome.tabs.onUpdated.addListener(function local(tabId,
                                                         changeInfo,
                                                         tab) {
          // Wait for the second tab to finish loading before moving on.
          if (tabId == secondTabId && changeInfo.status == 'complete') {
            chrome.tabs.onUpdated.removeListener(local);
            chrome.test.succeed();
          }
        });
      });
  },
  function removeSecondTab() {
    let onStoragePromise = new Promise(resolveOnStorageChanged);

    let removePromise = new Promise((resolve) => {
      chrome.tabs.remove(secondTabId, () => {
      resolve();
      });
    });

    Promise.all([onStoragePromise, removePromise]).then(chrome.test.succeed);
  }
])});
