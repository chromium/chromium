// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;

chrome.test.runTests([
  function getSelected() {
    chrome.tabs.getSelected(null, pass(function(tab) {
      assertEq(location.href, tab.url);
      assertEq(location.href, tab.title);
      firstWindowId = tab.windowId;
    }));
  },

  function create() {
    chrome.tabs.create({"windowId" : firstWindowId, "active" : false},
                       pass(function(tab){
      assertEq(1, tab.index);
      assertEq(firstWindowId, tab.windowId);
      assertEq(false, tab.selected);
      assertEq("chrome://newtab/", tab.pendingUrl);
      assertEq("", tab.url);
      // For most create calls the title will be an empty string until the
      // navigation commits, but the new tab page is an exception to this.
      assertEq("New Tab", tab.title);
      assertEq(false, tab.pinned);
      waitForAllTabs(pass(function() {
        chrome.tabs.get(tab.id, pass(function(tab) {
          assertEq("chrome://newtab/", tab.url);
          assertEq("New Tab", tab.title);
          assertEq(undefined, tab.pendingUrl);
        }));
      }));
    }));
  },

  function createInCurrent() {
    chrome.tabs.create({'windowId': chrome.windows.WINDOW_ID_CURRENT},
                       pass(function(tab) {
      chrome.windows.getCurrent(pass(function(win) {
        assertEq(win.id, tab.windowId);
      }));
    }));
  },

  function createInOtherWindow() {
    chrome.windows.create({}, pass(function(win) {
      // Create a tab in the older window.
      chrome.tabs.create({"windowId" : firstWindowId, "active" : false},
                         pass(function(tab) {
        assertEq(firstWindowId, tab.windowId);
      }));
      // Create a tab in this new window.
      chrome.tabs.create({"windowId" : win.id}, pass(function(tab) {
        assertEq(win.id, tab.windowId);
      }));
    }));
  },

  function createAtIndex() {
    chrome.tabs.create({"windowId" : firstWindowId, "index" : 1},
                       pass(function(tab) {
      assertEq(1, tab.index);
    }));
  },

  function createSelected() {
    chrome.tabs.create({"windowId" : firstWindowId, "active" : true},
                       pass(function(tab) {
      assertTrue(tab.active && tab.selected);
      chrome.tabs.getSelected(firstWindowId, pass(function(selectedTab) {
        assertEq(tab.id, selectedTab.id);
      }));
    }));
  },

  function createWindowWithDefaultTab() {
    var verify_default = function() {
      return pass(function(win) {
        assertEq(1, win.tabs.length);
        assertEq("chrome://newtab/", win.tabs[0].pendingUrl);
      });
    };

    // Make sure the window always has the NTP when no URL is supplied.
    chrome.windows.create({}, verify_default());
    chrome.windows.create({url:[]}, verify_default());
  },

  function createWindowWithExistingTab() {
    // Create a tab in the old window
    chrome.tabs.create({"windowId" : firstWindowId, "url": pageUrl('a'),
                        "active" : false},
                       pass(function(tab) {
      assertEq(firstWindowId, tab.windowId);
      assertEq(pageUrl('a'), tab.pendingUrl);

      // Create a new window with this tab
      chrome.windows.create({"tabId": tab.id}, pass(function(win) {
        assertEq(1, win.tabs.length);
        assertEq(tab.id, win.tabs[0].id);
        assertEq(win.id, win.tabs[0].windowId);
        assertEq(pageUrl('a'), win.tabs[0].pendingUrl);
      }));
    }));
  },

  function getAllInWindowNullArg() {
    chrome.tabs.getAllInWindow(null, pass(function(tabs) {
      assertEq(6, tabs.length);
      assertEq(firstWindowId, tabs[0].windowId);
    }));
  },

  function detectLanguage() {
    chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
      chrome.tabs.detectLanguage(tabs[0].id, pass(function(lang) {
        assertEq("und", lang);
      }));
    }));
  },

  function windowCreate() {
    chrome.windows.create({type: "popup"}, pass(function(window) {
      assertEq("popup", window.type);
      assertTrue(!window.incognito);
    }));
    chrome.windows.create({incognito: true}, pass(function(window) {
      // This extension is not incognito-enabled, so it shouldn't be able to
      // see the incognito window.
      assertEq(null, window);
    }));
  },

  function getCurrentWindow() {
    var errorMsg = "No window with id: -1.";
    chrome.windows.get(chrome.windows.WINDOW_ID_NONE, fail(errorMsg));
    chrome.windows.get(chrome.windows.WINDOW_ID_CURRENT, pass(function(win1) {
      chrome.windows.getCurrent(pass(function(win2) {
        assertEq(win1.id, win2.id);
      }));
    }));
  }

  /* Disabled -- see http://crbug.com/58229.
  function windowSetFocused() {
    chrome.windows.getCurrent(function(oldWin) {
      chrome.windows.create({}, function(newWin) {
        assertTrue(newWin.focused);
        chrome.windows.update(oldWin.id, {focused:true});
        chrome.windows.get(oldWin.id, pass(function(oldWin2) {
          assertTrue(oldWin2.focused);
        }));
      });
    });
  },
  */
]);
