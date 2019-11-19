// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var newTabUrls = [
  'chrome://newtab/',
  // The tab URL for to the Local New Tab Page.
  'chrome-search://local-ntp/local-ntp.html',
];

var secondWindowId;
var thirdWindowId;
var testTabId;

function clickLink(id) {
  var clickEvent = document.createEvent('MouseEvents');
  clickEvent.initMouseEvent('click', true, true, window);
  document.querySelector('#' + id).dispatchEvent(clickEvent);
}

chrome.test.runTests([

  function setupTwoWindows() {
    createWindow(["about:blank", "chrome://newtab/", pageUrl("a")], {},
                 pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        secondWindowId = winId;
        testTabId = tabIds[2];

        createWindow(["chrome://newtab/", pageUrl("b")], {},
                     pass(function(winId, tabIds) {
          waitForAllTabs(pass(function() {
            thirdWindowId = winId;
          }));
        }));
      }));
    }));
  },

  function getAllInWindow() {
    chrome.tabs.getAllInWindow(secondWindowId,
                               pass(function(tabs) {
      assertEq(3, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(secondWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);

        // The first tab should be active
        assertEq((i == 0), tabs[i].active && tabs[i].selected);
      }
      assertEq("about:blank", tabs[0].url);
      assertTrue(newTabUrls.includes(tabs[1].url));
      assertEq(pageUrl("a"), tabs[2].url);
    }));

    chrome.tabs.getAllInWindow(thirdWindowId,
                               pass(function(tabs) {
      assertEq(2, tabs.length);
      for (var i = 0; i < tabs.length; i++) {
        assertEq(thirdWindowId, tabs[i].windowId);
        assertEq(i, tabs[i].index);
      }
      assertTrue(newTabUrls.includes(tabs[0].url));
      assertEq(pageUrl("b"), tabs[1].url);
    }));
  },

  function updateSelect() {
    chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
      assertEq(true, tabs[0].active && tabs[0].selected);
      assertEq(false, tabs[1].active || tabs[1].selected);
      assertEq(false, tabs[2].active || tabs[2].selected);

      // Select tab[1].
      chrome.tabs.update(tabs[1].id, {active: true},
                         pass(function(tab1){
        // Check update of tab[1].
        chrome.test.assertEq(true, tab1.active);
        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(true, tabs[1].active && tabs[1].selected);
          assertEq(false, tabs[2].active || tabs[2].selected);
          // Select tab[2].
          chrome.tabs.update(tabs[2].id,
                             {active: true},
                             pass(function(tab2){
            // Check update of tab[2].
            chrome.test.assertEq(true, tab2.active);
            chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
              assertEq(false, tabs[1].active || tabs[1].selected);
              assertEq(true, tabs[2].active && tabs[2].selected);
            }));
          }));
        }));
      }));
    }));
  },

  function update() {
    chrome.tabs.get(testTabId, pass(function(tab) {
      assertEq(pageUrl("a"), tab.url);
      // Update url.
      chrome.tabs.update(testTabId, {"url": pageUrl("c")},
                         pass(function(tab){
        chrome.test.assertEq(pageUrl("a"), tab.url);
        chrome.test.assertEq('A', tab.title);
        chrome.test.assertEq(pageUrl("c"), tab.pendingUrl);
        waitForAllTabs(pass(function() {
          // Check url
          chrome.tabs.get(testTabId, pass(function(tab) {
            assertEq(pageUrl("c"), tab.url);
            assertEq('C', tab.title);
            assertEq(undefined, tab.pendingUrl);
          }));
        }));
      }));
    }));
  },

  function openerTabId() {
    chrome.test.listenOnce(
        chrome.tabs.onCreated,
        function(tab) {
      chrome.tabs.getCurrent(pass(function(thisTab) {
        assertEq(thisTab.id, tab.openerTabId);
      }));
    });
    // Pretend to click a link (openers aren't tracked when using tabs.create).
    clickLink("test_link");
  },

  // The window on chrome.tabs.create is ignored if it doesn't accept tabs.
  function testRedirectingToAnotherWindow() {
    chrome.windows.create(
        {url: 'about:blank', type: 'popup'},
        pass(function(window) {
      assertFalse(window.tabs[0].id == chrome.tabs.TAB_ID_NONE);
      chrome.tabs.create(
          {url: 'about:blank', windowId: window.id},
          pass(function(tab) {
        assertTrue(window.id != tab.windowId);
      }));
    }));
  },

  // Creation of a tab in an empty non-tabbed window should be allowed.
  function testOpenWindowInEmptyPopup() {
    chrome.windows.create(
        {type: 'popup'},
        pass(function(window) {
      chrome.tabs.create(
          {url: 'about:blank', windowId: window.id},
          pass(function(tab) {
        assertEq(window.id, tab.windowId);
      }));
    }));
  },

  // An empty popup window does not contain any tabs and the number of tabs
  // before and after creation should be the same.
  function testOpenEmptyPopup() {
    chrome.tabs.query({}, pass(function(tabs) {
      var tabsCountBefore = tabs.length;
      chrome.windows.create({type: 'popup'}, pass(function(window) {
        assertEq(window.tabs.length, 0);
        chrome.tabs.query({}, pass(function(tabs) {
          assertEq(tabsCountBefore, tabs.length);
        }));
      }));
    }));
  },

  function testCreatePopupAndMoveTab() {
    // An existing tab can be moved into a created empty popup.
    chrome.tabs.create({url: 'about:blank'}, pass(function(tab) {
      chrome.windows.create({type: 'popup', tabId: tab.id},
          pass(function(window) {
        assertEq(window.tabs.length, 1);
        chrome.tabs.get(tab.id, pass(function(updatedTabInfo) {
          assertEq(window.id, updatedTabInfo.windowId);
        }));
      }));
    }));

    // An existing tab cannot be moved into a created non-empty popup.
    chrome.tabs.create({url: 'about:blank'}, pass(function(tab) {
      chrome.windows.create({type: 'popup', url: 'about:blank', tabId: tab.id},
          pass(function(window) {
        assertEq(window.tabs.length, 1);
        chrome.tabs.get(tab.id, pass(function(updatedTabInfo) {
          assertEq(tab.windowId, updatedTabInfo.windowId);
          assertTrue(window.id != updatedTabInfo.windowId);
        }));
      }));
    }));
  },

]);
