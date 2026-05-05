// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let firstWindowId;

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.runTests([
    async function setupWindow() {
      await (new Promise(resolve => {
        createWindow(
            ['about:blank', 'chrome://newtab/', pageUrl('a')], {},
            function(winId, tabIds) {
              firstWindowId = winId;
              resolve();
            });
      }));
      chrome.test.succeed();
    },

    async function createPinned() {
      const winOptions = {windowId: firstWindowId, pinned: true};
      const updatedPromise = new Promise(resolve => {
        chrome.tabs.onUpdated.addListener(
            function listener(tabId, changeInfo, tab) {
              if ('pinned' in changeInfo) {
                assertEq(false, changeInfo.pinned);
                assertEq(false, tab.pinned);
                chrome.tabs.onUpdated.removeListener(listener);
                resolve();
              }
            });
      });

      // Create a new pinned tab, then unpin it, and ensure we get the proper
      // event.
      const tab = await chrome.tabs.create(winOptions);
      assertEq(true, tab.pinned);
      await chrome.tabs.update(tab.id, {pinned: false});
      await updatedPromise;

      // Leave a clean slate for the next test.
      await chrome.tabs.remove(tab.id);

      chrome.test.succeed();
    },

    async function updatePinned() {
      // A helper function that (un)pins a tab and verifies that both the
      // callback and the chrome.tabs.onUpdated event listeners are called.
      const pinTab = async function(id, pinnedState) {
        const updatedPromise = new Promise(async resolve => {
          chrome.tabs.onUpdated.addListener(
              function listener(tabId, changeInfo, tab) {
                if ('pinned' in changeInfo) {
                  assertEq(tabId, id);
                  assertEq(pinnedState, tab.pinned);
                  chrome.tabs.onUpdated.removeListener(listener);
                  resolve(tab);
                }
              });
        });
        const tab = await chrome.tabs.update(id, {pinned: pinnedState});
        assertEq(pinnedState, tab.pinned);

        return await updatedPromise;
      };

      // We pin and unpin these tabs because the TabStripModelObserver used to
      // have multiple notification code paths depending on the tab moves as a
      // result of being pinned or unpinned.
      // This works as follows:
      //   1.  Pin first tab (does not move, pinning)
      //   2.  Pin 3rd tab (moves to 2nd tab, pinning)
      //   3.  Unpin 1st tab (moves to 2nd tab, unpinning)
      //   4.  Unpin (new) 1st tab (does not move. unpinning)
      const tabs = await chrome.tabs.query({windowId: firstWindowId});
      assertEq(tabs.length, 3);
      for (let i = 0; i < tabs.length; i++) {
        assertEq(false, tabs[i].pinned);
      }

      let tab = await pinTab(tabs[0].id, true);
      assertEq(tabs[0].index, tab.index);
      tab = await pinTab(tabs[2].id, true);
      assertEq(1, tab.index);
      tab = await pinTab(tabs[0].id, false);
      assertEq(1, tab.index);
      tab = await pinTab(tabs[2].id, false);
      assertEq(0, tab.index);

      chrome.test.succeed();
    },
  ]);
});
