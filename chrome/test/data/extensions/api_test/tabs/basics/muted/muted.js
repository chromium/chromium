// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId_;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

// The set of tabs that have completed loading as of their last update.
const loadedTabs = new Set();
// A map of tabId -> Promise for any tabs we're waiting to finish loading.
const waitingForTabs = new Map();

// Waits for the given `tabId` to be done loading.
async function waitForTabLoaded(tabId) {
  if (loadedTabs.has(tabId)) {
    return;
  }

  const tabLoadedPromise = new Promise(resolve => {
    waitingForTabs.set(tabId, resolve);
  });

  await tabLoadedPromise;
}

// A top-level listener for tabs being updated, to listen for loaded state. This
// has to be here, since adding it after the tab is created might be too late if
// tabs load very quickly.
chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (tab.status == 'complete') {
    loadedTabs.add(tabId);
    if (waitingForTabs.has(tabId)) {
      const resolve = waitingForTabs.get(tabId);
      waitingForTabs.delete(tabId);
      resolve();
    }
  } else {
    loadedTabs.delete(tabId);
  }
});

loadScript.then(async function() {
chrome.test.runTests([
  async function createTab() {
    chrome.tabs.create({}, async (tab) => {
      testTabId_ = tab.id;
      // Wait for tab loading complete.
      await waitForTabLoaded(testTabId_);
      chrome.test.succeed();
    });
  },

  function mutedStartsFalse() {
    chrome.tabs.get(testTabId_, pass(function(tab) {
      assertEq(false, tab.mutedInfo.muted);

      queryForTab(testTabId_, {muted: false}, pass(function(tab) {
        assertEq(false, tab.mutedInfo.muted);
      }));
      queryForTab(testTabId_, {muted: true} , pass(function(tab) {
        assertEq(null, tab);
      }));
    }));
  },

  function makeMuted() {
    var expectedAfterMute = {
      muted: true,
      reason: 'extension',
      extensionId: chrome.runtime.id
    };

    chrome.tabs.onUpdated.addListener(function local(tabId, changeInfo, tab) {
      if (tabId != testTabId_ || !changeInfo.mutedInfo) {
        return;  // Ignore unrelated events.
      }
      assertEq(expectedAfterMute, changeInfo.mutedInfo);
      chrome.tabs.onUpdated.removeListener(local);
      chrome.test.succeed();
    });

    chrome.tabs.update(testTabId_, {muted: true});
  },

  function testStaysMutedAfterChangingWindow() {
    chrome.windows.create({}, function(window) {
      // chrome.tabs.onUpdated is not sent on tab movement.
      chrome.tabs.move(testTabId_, {windowId: window.id, index: -1},
                       function(tab) {
        assertEq(true, tab.mutedInfo.muted);
        chrome.test.succeed();
      });
    });
  },

  function makeNotMuted() {
    var expectedAfterUnmute = {
      muted: false,
      reason: 'extension',
      extensionId: chrome.runtime.id
    };

    chrome.tabs.onUpdated.addListener(function local(tabId, changeInfo, tab) {
      if (tabId != testTabId_ || !changeInfo.mutedInfo) {
        return;  // Ignore unrelated events.
      }
      chrome.tabs.onUpdated.removeListener(local);
      assertEq(expectedAfterUnmute, changeInfo.mutedInfo);
      chrome.test.succeed();
    });

    chrome.tabs.update(testTabId_, {muted: false});
  }

])});
