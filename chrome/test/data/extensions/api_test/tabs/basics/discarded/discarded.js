// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTab;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([

    function setupWindow() {
      var testTabId;

      createWindow(["about:blank", "chrome://newtab/"], {},
                   pass(function(winId, tabIds) {
        testTabId = tabIds[1];
      }));

      waitForAllTabs(pass(function() {
        queryForTab(testTabId, {}, pass(function(tab) {
          testTab = tab;
        }));
      }));
    },

    // Tests chrome.tabs.onUpdated for Discarded property.
    function discard() {
      // Initially tab isn't discarded.
      assertFalse(testTab.discarded);

      var onUpdatedCompleted = chrome.test.listenForever(
          chrome.tabs.onUpdated,
          function(tabId, changeInfo, tab) {
        if ('discarded' in changeInfo) {
          assertTrue('status' in changeInfo);

          // Make sure it's the right tab.
          assertEq(testTab.index, tab.index);
          assertEq(testTab.windowId, tab.windowId);

          // Make sure the discarded state changed correctly.
          assertTrue(changeInfo.discarded);
          assertTrue(tab.discarded);
          assertEq('unloaded', tab.status);

          onUpdatedCompleted();
        }
      });

      // TODO(georgesak): Remove tab update when http://crbug.com/632839 is
      // resolved.
      // Discard and update testTab (the id changes after a tab is discarded).
      chrome.tabs.discard(testTab.id, pass(function(tab) {
        assertTrue(tab.discarded);
        testTab = tab;
      }));
    },

  function reload() {
    // Tab is already discarded.
    assertTrue(testTab.discarded);

    var onUpdatedCompleted = chrome.test.listenForever(
        chrome.tabs.onUpdated,
        function(tabId, changeInfo, tab) {
      if ('discarded' in changeInfo) {
        // Make sure it's the right tab.
        assertEq(testTab.index, tab.index);
        assertEq(testTab.windowId, tab.windowId);

        // Make sure the discarded state changed correctly.
        assertFalse(changeInfo.discarded);
        assertFalse(tab.discarded);

        onUpdatedCompleted();
      }
    });

    chrome.tabs.reload(testTab.id);
  },

  // Tests chrome.tabs.onUpdated for autoDiscardable property.
  function setNonAutoDiscardable() {
    // Initially the tab is auto-discardable.
    assertTrue(testTab.autoDiscardable);

    var onUpdatedCompleted = chrome.test.listenForever(
        chrome.tabs.onUpdated,
        function(tabId, changeInfo, tab) {
      if ('autoDiscardable' in changeInfo) {
        // Make sure it's the right tab.
        assertEq(testTab.id, tab.id);

        // Make sure the auto-discardable state changed correctly.
        assertFalse(changeInfo.autoDiscardable);
        assertFalse(tab.autoDiscardable);

        onUpdatedCompleted();
      }
    });

    chrome.tabs.update(testTab.id, { autoDiscardable: false },
                       pass(function(tab) {
      assertFalse(tab.autoDiscardable);
      testTab = tab;
    }));
  },

  function resetAutoDiscardable() {
    // Tab was set to non auto-discardable.
    assertFalse(testTab.autoDiscardable);

    var onUpdatedCompleted = chrome.test.listenForever(
        chrome.tabs.onUpdated,
        function(tabId, changeInfo, tab) {
      if ('autoDiscardable' in changeInfo) {
        // Make sure it's the right tab.
        assertEq(testTab.id, tab.id);

        // Make sure the auto-discardable state changed correctly.
        assertTrue(changeInfo.autoDiscardable);
        assertTrue(tab.autoDiscardable);

        onUpdatedCompleted();
      }
    });

    chrome.tabs.update(testTab.id, { autoDiscardable: true },
                        pass(function (tab) {
      assertTrue(tab.autoDiscardable);
    }));
  }
])});
