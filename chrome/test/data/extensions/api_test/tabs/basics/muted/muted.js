// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId_;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function createTab() {
    chrome.tabs.create({}, function(tab) {
      testTabId_ = tab.id;
      // Wait for tab loading complete.
      chrome.tabs.onUpdated.addListener(function local(tabId, changeInfo, tab) {
        if (tabId != testTabId_ || changeInfo.status != 'complete') {
          return;
        }
        chrome.tabs.onUpdated.removeListener(local);
        chrome.test.succeed();
      })
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
