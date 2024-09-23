// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function setupWindow() {
    createWindow(["about:blank", "chrome://newtab/", pageUrl("a")], {},
                 pass(function(winId, tabIds) {
      firstWindowId = winId;
    }));
  },

  function createPinned() {
    var winOptions = {"windowId": firstWindowId, "pinned": true};
    var onUpdatedCompleted = chrome.test.listenForever(
      chrome.tabs.onUpdated,
      function(tabId, changeInfo, tab) {
        if ('pinned' in changeInfo) {
          assertEq(false, changeInfo.pinned);
          assertEq(false, tab.pinned);
          onUpdatedCompleted();
        }
      }
    );
    chrome.tabs.create(winOptions, pass(function(tab) {
      assertEq(true, tab.pinned);
      chrome.tabs.update(tab.id, {"pinned":false}, pass(function() {
        // Leave a clean slate for the next test.
        chrome.tabs.remove(tab.id);
      }));
    }));
  },

  function updatePinned() {
    // A helper function that (un)pins a tab and verifies that both the callback
    // and the chrome.tabs.onUpdated event listeners are called.
    var pinTab = function(id, pinnedState, callback) {
      var onUpdatedCompleted = chrome.test.listenForever(
        chrome.tabs.onUpdated,
        function(tabId, changeInfo, tab) {
          if ('pinned' in changeInfo) {
            assertEq(tabId, id);
            assertEq(pinnedState, tab.pinned);
            onUpdatedCompleted();
            if (callback)
              callback(tab);
          }
        }
      );
      chrome.tabs.update(id, { "pinned": pinnedState }, pass(function(tab) {
        assertEq(pinnedState, tab.pinned);
      }));
    };

    // We pin and unpin these tabs because the TabStripModelObserver used to
    // have multiple notification code paths depending on the tab moves as a
    // result of being pinned or unpinned.
    // This works as follows:
    //   1.  Pin first tab (does not move, pinning)
    //   2.  Pin 3rd tab (moves to 2nd tab, pinning)
    //   3.  Unpin 1st tab (moves to 2nd tab, unpinning)
    //   4.  Unpin (new) 1st tab (does not move. unpinning)
    chrome.tabs.query({windowId:firstWindowId},
      pass(function(tabs) {
        assertEq(tabs.length, 3);
        for (var i = 0; i < tabs.length; i++)
          assertEq(false, tabs[i].pinned);

        pinTab(tabs[0].id, true, function(tab) {
          assertEq(tabs[0].index, tab.index);
          pinTab(tabs[2].id, true, function(tab) {
            assertEq(1, tab.index);
            pinTab(tabs[0].id, false, function(tab) {
              assertEq(1, tab.index);
              pinTab(tabs[2].id, false, function (tab) {
                assertEq(0, tab.index);
              });
            });
          });
        });
      }));
  }
])});
