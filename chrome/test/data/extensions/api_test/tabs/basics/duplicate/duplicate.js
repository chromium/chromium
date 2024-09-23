// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstTabId;
var firstWindowId;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function setupWindow() {
    createWindow([pageUrl("a")], {},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        firstWindowId = winId;
        firstTabId = tabIds[0];
      }))
    }));
  },

  function duplicateTab() {
    chrome.tabs.duplicate(firstTabId, pass(function(tab) {
      waitForAllTabs(pass(function() {
        assertEq(pageUrl("a"), tab.url);
        assertEq(1, tab.index);
      }));
    }));
  },

  function totalTab() {
    chrome.tabs.query({windowId:firstWindowId},
      pass(function(tabs) {
        assertEq(tabs.length, 2);
        assertEq(tabs[0].url, tabs[1].url);
        assertEq(tabs[0].index + 1, tabs[1].index);
      }));
  },

  // TODO(crbug.com/40940015): This test was broken by crrev.com/c/3029302.
/*
  function duplicateTabFromNewPopupWindow() {
    chrome.windows.create({
        "url": "http://google.com",
        "type": "popup"
    },
    function(wnd) {
      var firstTab = wnd.tabs[0];
      chrome.tabs.duplicate(firstTab.id, pass(function(tab) {
        // Because the parent window is a popup, the duplicated tab will open
        // in a new window.
        assertTrue(wnd.id != tab.windowId);
      }));
    });
  }
 */
])});
