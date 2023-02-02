// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId_;

chrome.test.runTests([
  function setupWindow() {
    chrome.tabs.getCurrent(pass(function(tab) {
      testTabId_ = tab.id;
    }));
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
      assertEq(expectedAfterMute, changeInfo.mutedInfo);
      chrome.tabs.onUpdated.removeListener(local);
      chrome.test.succeed();
    });

    chrome.tabs.update(testTabId_, {muted: true});
  },

  function testStaysMutedAfterChangingWindow() {
    chrome.windows.create({}, function(window) {
      chrome.tabs.onUpdated.addListener(function local(tabId, changeInfo, tab) {
        if (changeInfo.status != 'complete')
          return;

        chrome.tabs.onUpdated.removeListener(local);
        chrome.test.succeed();
      });
      chrome.tabs.move(testTabId_, {windowId: window.id, index: -1},
                       function(tab) {
        assertEq(true, tab.mutedInfo.muted);
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
      chrome.tabs.onUpdated.removeListener(local);
      assertEq(expectedAfterUnmute, changeInfo.mutedInfo);
      chrome.test.succeed();
    });

    chrome.tabs.update(testTabId_, {muted: false});
  }

]);
