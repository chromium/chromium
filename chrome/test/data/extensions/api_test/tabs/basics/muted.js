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

    var callbackRunCount = 0;

    function checkWasMutedByThisExtension(tabId, changeInfo, tab) {
      assertEq(expectedAfterMute, changeInfo.mutedInfo);
      assertEq(expectedAfterMute, tab.mutedInfo);

      // Make sure both this and the callback provided to chrome.tabs.update()
      // below have been run at this point.
      ++callbackRunCount;
      assertEq(2, callbackRunCount);

      chrome.tabs.onUpdated.removeListener(checkWasMutedByThisExtension);
      chrome.test.succeed();
    }

    chrome.tabs.onUpdated.addListener(checkWasMutedByThisExtension);

    chrome.tabs.update(testTabId_, {muted: true}, function (tab) {
      ++callbackRunCount;
      assertEq(expectedAfterMute, tab.mutedInfo);
    });
  },

  function testStaysMutedAfterChangingWindow() {
    chrome.windows.create({}, pass(function(window) {
      chrome.tabs.move(testTabId_, {windowId: window.id, index: -1},
                       pass(function(tab) {
        assertEq(true, tab.mutedInfo.muted);
      }));
    }));
  },

  function makeNotMuted() {
    var expectedAfterUnmute = {
      muted: false,
      reason: 'extension',
      extensionId: chrome.runtime.id
    };

    var callbackRunCount = 0;

    function checkWasUnmutedByThisExtension(tabId, changeInfo, tab) {
      assertEq(expectedAfterUnmute, changeInfo.mutedInfo);
      assertEq(expectedAfterUnmute, tab.mutedInfo);

      // Make sure both this and the callback provided to chrome.tabs.update()
      // below have been run at this point.
      ++callbackRunCount;
      assertEq(2, callbackRunCount);

      chrome.tabs.onUpdated.removeListener(checkWasUnmutedByThisExtension);
      chrome.test.succeed();
    }

    chrome.tabs.onUpdated.addListener(checkWasUnmutedByThisExtension);

    chrome.tabs.update(testTabId_, {muted: false}, function (tab) {
      ++callbackRunCount;
      assertEq(expectedAfterUnmute, tab.mutedInfo);
    });
  }
]);
