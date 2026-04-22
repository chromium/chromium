// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let testWindowId;
const activeTabs = [];
const highlightedTabs = [];
const windowTabs = [];
const pinnedTabs = [];
const activeAndWindowTabs = [];

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);
let isAndroid;

loadScript.then(async function() {
  chrome.test.runTests([
    async function setup() {
      isAndroid = (await chrome.runtime.getPlatformInfo()).os === 'android';
      const tabs =
          ['http://example.org/a.html', 'http://www.google.com/favicon.ico'];
      chrome.windows.create({url: tabs}, pass(function(window) {
                              waitForAllTabs(pass(function() {
                                assertEq(2, window.tabs.length);
                                testWindowId = window.id;
                                chrome.tabs.create(
                                    {
                                      windowId: testWindowId,
                                      url: 'about:blank',
                                      pinned: true,
                                    },
                                    pass(function(tab) {
                                      waitForAllTabs(pass(function() {
                                        // TODO(crbug.com/371432155): Android
                                        // does not yet support creating pinned
                                        // tabs with chrome.tabs.create(). See
                                        // OpenTabHelper.
                                        if (!isAndroid) {
                                          assertTrue(tab.pinned);
                                        }
                                        assertEq(testWindowId, tab.windowId);
                                      }));
                                    }));
                              }));
                            }));
    },

    function queryAll() {
      chrome.tabs.query({}, pass(function(tabs) {
                          assertEq(4, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            if (tabs[x].highlighted) {
                              highlightedTabs.push(tabs[x]);
                            }
                            if (tabs[x].active) {
                              activeTabs.push(tabs[x]);
                            }
                            if (tabs[x].windowId == testWindowId) {
                              windowTabs.push(tabs[x]);
                              if (tabs[x].active) {
                                activeAndWindowTabs.push(tabs[x]);
                              }
                            }
                            if (tabs[x].pinned) {
                              pinnedTabs.push(tabs[x]);
                            }
                          }
                        }));
    },

    function queryHighlighted() {
      chrome.tabs.query({highlighted: true}, pass(function(tabs) {
                          assertEq(highlightedTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertTrue(tabs[x].highlighted);
                          }
                        }));
      chrome.tabs.query({highlighted: false}, pass(function(tabs) {
                          assertEq(4 - highlightedTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertFalse(tabs[x].highlighted);
                          }
                        }));
    },

    function queryActive() {
      chrome.tabs.query({active: true}, pass(function(tabs) {
                          assertEq(activeTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertTrue(tabs[x].active);
                          }
                        }));
      chrome.tabs.query({active: false}, pass(function(tabs) {
                          assertEq(4 - activeTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertFalse(tabs[x].active);
                          }
                        }));
    },

    function queryWindowID() {
      chrome.tabs.query({windowId: testWindowId}, pass(function(tabs) {
                          assertEq(windowTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertEq(testWindowId, tabs[x].windowId);
                          }
                        }));
    },

    function queryPinned() {
      // TODO(crbug.com/371432155): Android does not yet support creating
      // pinned tabs with chrome.tabs.create(). See OpenTabHelper.
      if (isAndroid) {
        chrome.test.succeed('skipped');
        return;
      }
      chrome.tabs.query({pinned: true}, pass(function(tabs) {
                          assertEq(pinnedTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertTrue(tabs[x].pinned);
                          }
                        }));
      chrome.tabs.query({pinned: false}, pass(function(tabs) {
                          assertEq(4 - pinnedTabs.length, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            assertFalse(tabs[x].pinned);
                          }
                        }));
    },

    function queryActiveAndWindowID() {
      chrome.tabs.query(
          {
            active: true,
            windowId: testWindowId,
          },
          pass(function(tabs) {
            assertEq(activeAndWindowTabs.length, tabs.length);
            for (let x = 0; x < tabs.length; x++) {
              assertTrue(tabs[x].active);
              assertEq(testWindowId, tabs[x].windowId);
            }
          }));
    },

    function queryUrl() {
      chrome.tabs.query({url: 'http://*.example.org/*'}, pass(function(tabs) {
                          assertEq(1, tabs.length);
                          assertEq('http://example.org/a.html', tabs[0].url);
                        }));
    },

    function queryUrlAsArray() {
      chrome.tabs.query({url: ['http://*.example.org/*']}, pass(function(tabs) {
                          assertEq(1, tabs.length);
                          assertEq('http://example.org/a.html', tabs[0].url);
                        }));
    },

    function queryUrlAsArray2() {
      chrome.tabs.query(
          {url: ['http://*.example.org/*', '*://*.google.com/*']},
          pass(function(tabs) {
            assertEq(2, tabs.length);
            assertEq('http://example.org/a.html', tabs[0].url);
            assertEq('http://www.google.com/favicon.ico', tabs[1].url);
          }));
    },

    function queryStatus() {
      chrome.tabs.query({status: 'complete'}, pass(function(tabs) {
                          for (let x = 0; x < tabs.length; x++) {
                            assertEq('complete', tabs[x].status);
                          }
                        }));
    },

    function queryWindowType() {
      chrome.tabs.query({windowType: 'normal'}, pass(function(tabs) {
                          assertEq(4, tabs.length);
                          for (let x = 0; x < tabs.length; x++) {
                            chrome.windows.get(
                                tabs[x].windowId, pass(function(win) {
                                  assertTrue(win.type == 'normal');
                                  assertEq(false, win.alwaysOnTop);
                                }));
                          }
                        }));
      chrome.windows.create(
          {
            url: 'about:blank',
            type: 'popup',
          },
          pass(function(win) {
            chrome.windows.create(
                {
                  url: 'about:blank',
                  type: 'popup',
                },
                pass(function(win) {
                  chrome.tabs.query(
                      {
                        windowId: win.id,
                        windowType: 'popup',
                      },
                      pass(function(tabs) {
                        assertEq(1, tabs.length);
                      }));
                  chrome.tabs.query(
                      {windowType: 'popup'}, pass(function(tabs) {
                        assertEq(2, tabs.length);
                        for (let i = 0; i < tabs.length; i++) {
                          assertFalse(tabs[i].id == chrome.tabs.TAB_ID_NONE);
                        }
                      }));
                  chrome.tabs.query(
                      {
                        windowType: 'popup',
                        url: 'about:*',
                      },
                      pass(function(tabs) {
                        assertEq(2, tabs.length);
                        for (let i = 0; i < tabs.length; i++) {
                          assertFalse(tabs[i].id == chrome.tabs.TAB_ID_NONE);
                        }
                      }));
                }));
          }));
    },

    function queryIndex() {
      chrome.tabs.query({index: 0}, pass(function(tabs) {
                          // Each of the 4 windows should have a tab at index 0.
                          assertEq(4, tabs.length);
                          for (let i = 0; i < tabs.length; i++) {
                            assertEq(0, tabs[i].index);
                          }
                        }));
    },

    function queryTitle() {
      const titleUrl = chrome.runtime.getURL('query.html');
      chrome.tabs.create({url: titleUrl}, pass(function() {
                           waitForAllTabs(pass(function() {
                             chrome.tabs.query(
                                 {title: '*query.html'}, pass(function(tabs) {
                                   assertEq(1, tabs.length);
                                   assertEq(titleUrl, tabs[0].title);
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
    },
  ]);
});
