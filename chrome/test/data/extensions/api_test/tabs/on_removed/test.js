// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// tabs api test: removing windows.
// browser_tests.exe --gtest_filter=ExtensionApiTest.TabOnRemoved

// We have a bunch of places where we need to remember some state from one
// test (or setup code) to subsequent tests.
var firstWindowId = null;
var secondWindowId = null;

var windowEventsWindow = null;
var moveTabIds = {};

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

// Utility functions to help with tabs/windows testing.

// Removes current windows and creates one window with tabs set to
// the urls in the array |tabUrls|. At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function setupWindow(tabUrls, callback) {
  createWindow(tabUrls, {}, function(winId, tabIds) {
    // Remove all other windows.
    var removedCount = 0;
    chrome.windows.getAll({}, function(windows) {
      for (var i in windows) {
        if (windows[i].id != winId) {
          chrome.windows.remove(windows[i].id, function() {
            removedCount++;
            if (removedCount == windows.length - 1)
              callback(winId, tabIds);
          });
        }
      }
      if (windows.length == 1)
        callback(winId, tabIds);
    });
  });
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, winOptions, callback) {
  winOptions["url"] = tabUrls;
  chrome.windows.create(winOptions, function(win) {
    var newTabIds = [];
    assertTrue(win.id > 0);
    assertEq(tabUrls.length, win.tabs.length);

    for (var i = 0; i < win.tabs.length; i++) {
      newTabIds.push(win.tabs[i].id);
    }

    callback(win.id, newTabIds);
  });
}

function pageUrl(letter) {
  return chrome.runtime.getURL(letter + ".html");
}

chrome.test.runTests([
  // Open some pages, so that we can try to close them.
  function setupLetterPages() {
    var pages = ["chrome://newtab/", pageUrl('a'), pageUrl('b'),
                   pageUrl('c'), pageUrl('d'), pageUrl('e')];
    setupWindow(pages, pass(function(winId, tabIds) {
      firstWindowId = winId;
      moveTabIds['a'] = tabIds[1];
      moveTabIds['b'] = tabIds[2];
      moveTabIds['c'] = tabIds[3];
      moveTabIds['d'] = tabIds[4];
      moveTabIds['e'] = tabIds[5];
      createWindow(["chrome://newtab/"], {}, pass(function(winId, tabIds) {
        secondWindowId = winId;
      }));
      chrome.tabs.query({windowId:firstWindowId}, pass(function(tabs) {
        assertEq(pages.length, tabs.length);
        for (var i in tabs) {
          assertEq(pages[i], tabs[i].url || tabs[i].pendingUrl);
        }
      }));
    }));
  },

  function tabsOnRemoved() {
    chrome.test.listenOnce(chrome.tabs.onRemoved,
      function(tabid, removeInfo) {
        assertEq(moveTabIds['c'], tabid);
        assertEq(firstWindowId, removeInfo.windowId);
        assertEq(false, removeInfo.isWindowClosing);
    });

    chrome.tabs.remove(moveTabIds['c'], pass());
  },

  function windowsOnCreated() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      windowEventsWindow = window;
      chrome.tabs.query({windowId:window.id}, pass(function(tabs) {
        assertEq(pageUrl("a"), tabs[0].url || tabs[0].pendingUrl);
      }));
    });

    chrome.windows.create({"url": pageUrl("a")}, pass(function(tab) {}));
  },

  function windowsOnRemoved() {
    chrome.test.listenOnce(chrome.windows.onRemoved, function(windowId) {
      assertEq(windowEventsWindow.id, windowId);
    });

    chrome.test.listenOnce(chrome.tabs.onRemoved,
      function(tabId, removeInfo) {
        assertEq(windowEventsWindow.id, removeInfo.windowId);
        assertEq(true, removeInfo.isWindowClosing);
    });

    chrome.windows.remove(windowEventsWindow.id, pass());
  }
]);
