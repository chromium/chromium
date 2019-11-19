// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

function pageUrl(letter) {
  return chrome.extension.getURL(letter + ".html");
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

    for (var i = 0; i < win.tabs.length; i++)
      newTabIds.push(win.tabs[i].id);

    callback(win.id, newTabIds);
  });
}

// Waits until all tabs (yes, in every window) have status "complete".
// This is useful to prevent test overlap when testing tab events.
// |callback| should look like function() {...}.  Note that |callback| expects
// zero arguments.
function waitForAllTabs(callback) {
  // Wait for all tabs to load.
  function waitForTabs() {
    chrome.windows.getAll({"populate": true}, function(windows) {
      var ready = true;
      for (var i in windows) {
        for (var j in windows[i].tabs) {
          if (windows[i].tabs[j].status != "complete") {
            ready = false;
            break;
          }
        }
        if (!ready)
          break;
      }
      if (ready)
        callback();
      else
        window.setTimeout(waitForTabs, 30);
    });
  }
  waitForTabs();
}

// Like chrome.tabs.query, but with the ability to filter by |tabId| as well.
// Returns the found tab or null
function queryForTab(tabId, queryInfo, callback) {
  chrome.tabs.query(queryInfo,
    pass(function(tabs) {
      var foundTabs = tabs.filter(function(tab) {
        return (tab.id == tabId);
      });
      if (callback !== null)
        callback(foundTabs.length ? foundTabs[0] : null);
    })
  );
}

// Check onUpdated for a queryable attribute such as muted or audible
// and then check that the tab, a query, and changeInfo are consistent
// with the expected value.  Does similar checks for each
// (nonqueryable attribute, expected value) pair in nonqueryableAttribsDict
// except it does not check the query.
function onUpdatedExpect(queryableAttrib, expected, nonqueryableAttribsDict) {
  var onUpdatedCompleted = chrome.test.listenForever(
    chrome.tabs.onUpdated,
    function(tabId, changeInfo, tab) {
      if (nonqueryableAttribsDict !== null) {
        var nonqueryableAttribs = Object.keys(nonqueryableAttribsDict);
        nonqueryableAttribs.forEach(function(nonqueryableAttrib) {
          if (typeof changeInfo[nonqueryableAttrib] !== "undefined") {
            assertEq(nonqueryableAttribsDict[nonqueryableAttrib],
                     changeInfo[nonqueryableAttrib]);
            assertEq(nonqueryableAttribsDict[nonqueryableAttrib],
                     tab[nonqueryableAttrib]);
          }
        });
      }
      if (changeInfo.hasOwnProperty(queryableAttrib)) {
        assertEq(expected, changeInfo[queryableAttrib]);
        assertEq(expected, tab[queryableAttrib]);
        var queryInfo = {};
        queryInfo[queryableAttrib] = expected;
        queryForTab(tabId, queryInfo, pass(function(tab) {
          assertEq(expected, tab[queryableAttrib]);
          queryInfo[queryableAttrib] = !expected;

          queryForTab(tabId, queryInfo, pass(function(tab) {
            assertEq(null, tab);
            onUpdatedCompleted();
          }));
        }));
      }
    }
  );
}

// Create one window with names. It returns created windowId and tabIdsTable
// that can be used to find tab's id by page name.
function setupWindow(pageNames) {
  const pages = pageNames.map(pageName => pageUrl(pageName));
  return new Promise(resolve => {
    createWindow(pages, {}, function(winId, tabIds) {
      const tabIdsTable = {};
      for (let i = 0; i < tabIds.length; i++) {
        tabIdsTable[pageNames[i]] = tabIds[i];
      };
      resolve([winId, tabIdsTable]);
    });
  });
};