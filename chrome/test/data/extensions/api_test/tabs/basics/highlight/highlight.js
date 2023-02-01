// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testWindowId1, testWindowId2;

function contains(arr, value) {
  return arr.some(function(element) { return element == value; });
}

function checkEqualSets(set1, set2) {
  if (set1.length != set2.length)
    return false;

  for (var x = 0; x < set1.length; x++) {
    if (!set2.some(function(v) { return v == set1[x]; }))
      return false;
  }

  return true;
}

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function setup() {
    var tabs1 = ['http://e.com', 'http://a.com', 'http://a.com/b.html',
                 'http://b.com', 'http://a.com/d.html', 'http://a.com/c.html'];
    var tabs2 = ['http://c.com/', 'http://a.com', 'http://a.com/b.html'];
    chrome.windows.create({url: tabs1}, pass(function(win) {
      testWindowId1 = win.id;
    }));
    chrome.windows.create({url: tabs2}, pass(function(win) {
      testWindowId2 = win.id;
    }));
  },

  function highlightCurrentWindow() {
    // Check that omitting the windowId highlights the current window
    chrome.windows.getCurrent(pass(function(win1) {
      chrome.tabs.highlight({tabs: [0]}, pass(function(win2) {
        assertEq(win1.id, win2.id);
      }));
    }));
  },

  function highlightA() {
    chrome.tabs.query({windowId: testWindowId1, url: 'http://a.com/*'},
                      pass(function(tabs) {
      assertEq(4, tabs.length);
      // Note: tabs.onHighlightChanged is deprecated.
      chrome.test.listenOnce(chrome.tabs.onHighlightChanged,
                             function(highlightInfo) {
        var tabIds = tabs.map(function(tab) { return tab.id; });
        assertEq(highlightInfo.windowId, testWindowId1);
        assertTrue(checkEqualSets(tabIds, highlightInfo.tabIds));
      });
      var tabIndices = tabs.map(function(tab) { return tab.index; });
      chrome.tabs.highlight({
        windowId: testWindowId1,
        tabs: tabIndices
      }, pass(function(win) {
        // Verify the 'highlighted' property for every tab.
        win.tabs.forEach(function(tab) {
          assertEq(contains(tabIndices, tab.index), tab.highlighted);
        });
      }));
    }));
  },

  function highlightB() {
    chrome.tabs.query({windowId: testWindowId1, url: 'http://b.com/*'},
                      pass(function(tabs) {
      assertEq(1, tabs.length);
      chrome.test.listenOnce(chrome.tabs.onHighlighted,
                             function(highlightInfo) {
        var tabIds = tabs.map(function(tab) { return tab.id; });
        assertEq(highlightInfo.windowId, testWindowId1);
        assertTrue(checkEqualSets(tabIds, highlightInfo.tabIds));
      });
      var tabIndices = tabs.map(function(tab) { return tab.index; });
      chrome.tabs.highlight({windowId: testWindowId1, tabs: tabIndices},
                         pass(function(win) {
        // Verify the 'highlighted' property for every tab.
        win.tabs.forEach(function(tab) {
          assertEq(contains(tabIndices, tab.index), tab.highlighted);
        });
      }));
    }));
  },

  function highlightAWindow2() {
    chrome.tabs.query({windowId: testWindowId2, url: 'http://a.com/*'},
                      pass(function(tabs) {
      assertEq(2, tabs.length);
      chrome.test.listenOnce(chrome.tabs.onHighlighted,
                             function(highlightInfo) {
        var tabIds = tabs.map(function(tab) { return tab.id; });
        assertEq(highlightInfo.windowId, testWindowId2);
        assertTrue(checkEqualSets(tabIds, highlightInfo.tabIds));
      });
      var tabIndices = tabs.map(function(tab) { return tab.index; });
      chrome.tabs.highlight({windowId: testWindowId2, tabs: tabIndices},
                         pass(function(win) {
        // Verify the 'highlighted' property for every tab.
        win.tabs.forEach(function(tab) {
          assertEq(contains(tabIndices, tab.index), tab.highlighted);
        });

        // Verify that nothing has changed in window 1.
        chrome.tabs.query({windowId: testWindowId1, highlighted: true},
                          pass(function(tabs) {
          assertEq(1, tabs.length);
        }));
      }));
    }));
  },

  function removeTab() {
    chrome.tabs.query(
        {windowId: testWindowId2, highlighted: true, active: false},
        pass(function(tabs) {
      var tabId = tabs[0].id;
      chrome.test.listenOnce(chrome.tabs.onHighlighted,
                             function(highlightInfo) {
        assertEq(1, highlightInfo.tabIds.length);
        assertTrue(tabId != highlightInfo.tabIds[0]);
      });
      chrome.tabs.remove(tabId, pass(function() { assertTrue(true); }));
    }));
  },

  function noTabsHighlighted() {
    chrome.tabs.highlight({windowId: testWindowId1, tabs: []},
                       fail("No highlighted tab"));
  },

  function indexNotFound() {
    chrome.tabs.highlight({windowId: testWindowId1, tabs: [3333]},
                       fail("No tab at index: 3333."));
  }
])});
