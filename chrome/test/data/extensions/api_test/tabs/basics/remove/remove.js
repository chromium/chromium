// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function resolveOnStorageChanged(resolve) {
  chrome.storage.local.onChanged.addListener(function local(changes,
                                                            areaName) {
    assertEq({newValue: 'yes'}, changes['did_run_unload_1'])
    chrome.storage.local.onChanged.removeListener(local);
    resolve();
  });
}

let secondTabId;
const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
chrome.test.runTests([
  async function createSecondTab() {
    const tabReadyPromise = new Promise((resolve) => {
      const onUpdatedListener = function(tabId, changeInfo, tab) {
        if (changeInfo.status === 'complete' &&
            tab.url.includes('unload-storage-1')) {
          chrome.tabs.onUpdated.removeListener(onUpdatedListener);
          resolve();
        }
      };
      chrome.tabs.onUpdated.addListener(onUpdatedListener);
    });

    const tab = await chrome.tabs.create(
                    {index: 1, active: true, url: pageUrl('unload-storage-1')});
    secondTabId = tab.id;
    assertTrue(tab.active);
    assertEq(1, tab.index);

    await tabReadyPromise;
    chrome.test.succeed();
  },

  async function removeSecondTab() {
    const onStoragePromise = new Promise(resolveOnStorageChanged);
    const tabRemoved = chrome.tabs.remove(secondTabId);
    await Promise.all([onStoragePromise, tabRemoved]);
    chrome.test.succeed();
  }
])});
