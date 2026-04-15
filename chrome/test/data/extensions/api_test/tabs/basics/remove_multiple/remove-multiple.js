// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let firstTabId;
let secondTabId;
let thirdTabId;
let fourthTabId;

function resolveOnStorageChanged(key, resolve) {
  chrome.storage.local.onChanged.addListener(function local(changes,
                                                            areaName) {
    const change = changes[key];
    if (change == undefined)
      return;
    assertEq({newValue: 'yes'}, change)
    chrome.storage.local.onChanged.removeListener(local);
    resolve();
  });
}

function createTab(createParams) {
  return new Promise((resolve) => {
    chrome.tabs.create(createParams, (tab) => {
      const createdTabId = tab.id;
      chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
        // Wait for the tab to finish loading.
        if (tabId == createdTabId && changeInfo.status == 'complete') {
          resolve(tab);
        }
      });
    });
  });
}

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
chrome.test.runTests([
  function getFirstTabId() {
    chrome.tabs.query({ windowId: chrome.windows.WINDOW_ID_CURRENT },
      (tabs) => {
        // Make sure we start the test with one tab.
        assertEq(1, tabs.length);
        firstTabId = tabs[0].id;
        chrome.test.succeed();
    })
  },
  function createTabs() {
    // Create a second tab that has an unload handler.
    createTab({index: 1, active: false, url: pageUrl('unload-storage-1')})
      .then((tab) => {
        secondTabId = tab.id;
        assertFalse(tab.active);
        assertEq(1, tab.index);
        // Create and switch to a third tab that has an unload handler.
        return createTab(
            {index: 2, active: true, url: pageUrl('unload-storage-2')});
      }).then((tab) => {
        thirdTabId = tab.id;
        assertTrue(tab.active);
        assertEq(2, tab.index);
        // Create a fourth tab that does not have an unload handler (it will
        // open the default New Tab Page).
        return createTab({index: 3, active: false });
      }).then((tab) => {
        fourthTabId = tab.id;
        assertFalse(tab.active);
        assertEq(3, tab.index);
        chrome.test.succeed();
      });
  },
  function removeCreatedTabs() {
    const onStorageChangedPromise1 =
        new Promise(resolveOnStorageChanged.bind(this, 'did_run_unload_1'));
    const onStorageChangedPromise2 =
        new Promise(resolveOnStorageChanged.bind(this, 'did_run_unload_2'));

    const removePromise = new Promise((resolve) => {
      chrome.tabs.remove([secondTabId, thirdTabId, fourthTabId], () => {
        chrome.tabs.query({ windowId: chrome.windows.WINDOW_ID_CURRENT },
          (tabs) => {
            // Make sure we only have one tab left (the first tab) in the
            // window.
            assertEq(1, tabs.length);
            assertEq(firstTabId, tabs[0].id);
            resolve();
          });
      });
    });

    Promise.all([onStorageChangedPromise1, onStorageChangedPromise2,
                 removePromise]).then(chrome.test.succeed);
  }
])});
