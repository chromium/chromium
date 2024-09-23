// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testWindowId;
var active_tabs = [];
var highlighted_tabs = [];
var window_tabs = [];
var pinned_tabs = [];
var active_and_window_tabs = [];

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function setup() {
    var tabs = ['http://example.org/a.html', 'http://www.google.com/favicon.ico'];
    chrome.windows.create({url: tabs}, pass(function(window) {
      waitForAllTabs(pass(function() {
        assertEq(2, window.tabs.length);
        testWindowId = window.id;
        chrome.tabs.create({
          windowId: testWindowId,
          url: 'about:blank',
          pinned: true
        }, pass(function(tab) {
          waitForAllTabs(pass(function() {
            assertTrue(tab.pinned);
            assertEq(testWindowId, tab.windowId);
          }));
        }));
      }));
    }));
  },

  function queryAll() {
    chrome.tabs.query({}, pass(function(tabs) {
      assertEq(4, tabs.length);
      for (var x = 0; x < tabs.length; x++) {
        if (tabs[x].highlighted)
          highlighted_tabs.push(tabs[x]);
        if (tabs[x].active)
          active_tabs.push(tabs[x]);
        if (tabs[x].windowId == testWindowId) {
          window_tabs.push(tabs[x]);
          if (tabs[x].active)
            active_and_window_tabs.push(tabs[x]);
        }
        if (tabs[x].pinned)
          pinned_tabs.push(tabs[x]);
      }
    }));
  },

  function queryHighlighted() {
    chrome.tabs.query({highlighted:true}, pass(function(tabs) {
      assertEq(highlighted_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertTrue(tabs[x].highlighted);
    }));
    chrome.tabs.query({highlighted:false}, pass(function(tabs) {
      assertEq(4-highlighted_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertFalse(tabs[x].highlighted);
    }));
  },

  function queryActive() {
    chrome.tabs.query({active: true}, pass(function(tabs) {
      assertEq(active_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertTrue(tabs[x].active);
    }));
    chrome.tabs.query({active: false}, pass(function(tabs) {
      assertEq(4-active_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertFalse(tabs[x].active);
    }));
  },

  function queryWindowID() {
    chrome.tabs.query({windowId: testWindowId}, pass(function(tabs) {
      assertEq(window_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertEq(testWindowId, tabs[x].windowId);
    }));
  },

  function queryPinned() {
    chrome.tabs.query({pinned: true}, pass(function(tabs) {
      assertEq(pinned_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertTrue(tabs[x].pinned);
    }));
    chrome.tabs.query({pinned: false}, pass(function(tabs) {
      assertEq(4-pinned_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++)
        assertFalse(tabs[x].pinned);
    }));
  },

  function queryActiveAndWindowID() {
    chrome.tabs.query({
      active: true,
      windowId: testWindowId
    }, pass(function(tabs) {
      assertEq(active_and_window_tabs.length, tabs.length);
      for (var x = 0; x < tabs.length; x++) {
        assertTrue(tabs[x].active);
        assertEq(testWindowId, tabs[x].windowId);
      }
    }));
  },

  function queryUrl() {
    chrome.tabs.query({url: "http://*.example.org/*"}, pass(function(tabs) {
      assertEq(1, tabs.length);
      assertEq("http://example.org/a.html", tabs[0].url);
    }));
  },

  function queryUrlAsArray() {
    chrome.tabs.query({url: ["http://*.example.org/*"]}, pass(function(tabs) {
      assertEq(1, tabs.length);
      assertEq("http://example.org/a.html", tabs[0].url);
    }));
  },

  function queryUrlAsArray2() {
    chrome.tabs.query({url: ["http://*.example.org/*", "*://*.google.com/*"]}, pass(function(tabs) {
      assertEq(2, tabs.length);
      assertEq("http://example.org/a.html", tabs[0].url);
      assertEq("http://www.google.com/favicon.ico", tabs[1].url);
    }));
  },

  function queryStatus() {
    chrome.tabs.query({status: "complete"}, pass(function(tabs) {
      for (var x = 0; x < tabs.length; x++)
        assertEq("complete", tabs[x].status);
    }));
  },

  function queryWindowType() {
    chrome.tabs.query({windowType: "normal"}, pass(function(tabs) {
      assertEq(4, tabs.length);
      for (var x = 0; x < tabs.length; x++) {
        chrome.windows.get(tabs[x].windowId, pass(function(win) {
          assertTrue(win.type == "normal");
          assertEq(false, win.alwaysOnTop);
        }));
      }
    }));
    chrome.windows.create({
      url: 'about:blank',
      type: 'popup'
    }, pass(function(win) {
      chrome.windows.create({
        url: 'about:blank',
        type: 'popup'
      }, pass(function(win) {
        chrome.tabs.query({
          windowId: win.id,
          windowType: 'popup'
        }, pass(function(tabs) {
          assertEq(1, tabs.length);
        }));
        chrome.tabs.query({windowType: 'popup'}, pass(function(tabs) {
          assertEq(2, tabs.length);
          for (var i = 0; i < tabs.length; i++)
            assertFalse(tabs[i].id == chrome.tabs.TAB_ID_NONE);
        }));
        chrome.tabs.query({
          windowType: 'popup',
          url: 'about:*'
        }, pass(function(tabs) {
          assertEq(2, tabs.length);
          for (var i = 0; i < tabs.length; i++)
            assertFalse(tabs[i].id == chrome.tabs.TAB_ID_NONE);
        }));
      }));
    }));
  },

  function queryIndex() {
    chrome.tabs.query({index: 0}, pass(function(tabs) {
      // Each of the 4 windows should have a tab at index 0.
      assertEq(4, tabs.length);
      for (var i = 0; i < tabs.length; i++)
        assertEq(0, tabs[i].index);
    }));
  },

  function queryTitle() {
    let title_url = chrome.runtime.getURL("query.html");
    chrome.tabs.create({url: title_url}, pass(function() {
      waitForAllTabs(pass(function() {
        chrome.tabs.query({title: "*query.html"}, pass(function(tabs) {
          assertEq(1, tabs.length);
          assertEq(title_url, tabs[0].title);
        }));
      }));
    }));
  },

  function queryIncognito() {
    chrome.windows.create(
        {url: ['http://a.com', 'http://a.com'], incognito: true},
        pass(function(win) {
      assertEq(null, win);
      chrome.tabs.query({url: 'http://a.com/'}, pass(function(tabs) {
         assertEq(0, tabs.length);
      }));
    }));
  }
])});
