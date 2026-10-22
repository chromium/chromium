// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let firstWindowId;
let secondWindowId;
const moveTabIds = {};
const CHROME_UI_NEW_TAB_URL = 'chrome://newtab/';

const newTabUrls = [
  CHROME_UI_NEW_TAB_URL,
  // The tab URL for the Local New Tab Page.
  'chrome-search://local-ntp/local-ntp.html',
];

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.runTests([
    // Do a series of moves and removes so that we get the following
    //
    // Before:
    //  Window1: (newtab),a,b,c,d,e
    //  Window2: (newtab)
    //
    // After moveToInvalidTab:
    //  Window1: (newtab)
    //  Window2: b,a,(newtab)
    function setupLetterPages() {
      const pages = [
        CHROME_UI_NEW_TAB_URL,
        pageUrl('a'),
        pageUrl('b'),
        pageUrl('c'),
        pageUrl('d'),
        pageUrl('e'),
      ];
      createWindow(
          pages, {}, pass(function(winId, tabIds) {
            waitForAllTabs(pass(function() {
              firstWindowId = winId;
              moveTabIds['a'] = tabIds[1];
              moveTabIds['b'] = tabIds[2];
              moveTabIds['c'] = tabIds[3];
              moveTabIds['d'] = tabIds[4];
              moveTabIds['e'] = tabIds[5];
              createWindow(
                  [CHROME_UI_NEW_TAB_URL], {}, pass(function(winId, tabIds) {
                    waitForAllTabs(pass(function() {
                      secondWindowId = winId;
                    }));
                  }));
              chrome.tabs.query({windowId: firstWindowId}, pass(function(tabs) {
                                  assertEq(pages.length, tabs.length);
                                  assertTrue(newTabUrls.includes(tabs[0].url));
                                  for (let i = 1; i < tabs.length; i++) {
                                    assertEq(pages[i], tabs[i].url);
                                  }
                                }));
            }));
          }));
    },

    function move() {
      // Check that the tab/window state is what we expect after doing moves.
      function checkMoveResults() {
        chrome.tabs.query({windowId: firstWindowId}, pass(function(tabs) {
                            assertEq(4, tabs.length);
                            assertTrue(newTabUrls.includes(tabs[0].url));
                            assertEq(pageUrl('a'), tabs[1].url);
                            assertEq(pageUrl('e'), tabs[2].url);
                            assertEq(pageUrl('c'), tabs[3].url);

                            chrome.tabs.query(
                                {windowId: secondWindowId},
                                pass(function(tabs) {
                                  assertEq(3, tabs.length);
                                  assertEq(pageUrl('b'), tabs[0].url);
                                  assertTrue(newTabUrls.includes(tabs[1].url));
                                  assertEq(pageUrl('d'), tabs[2].url);
                                }));
                          }));
      }

      chrome.tabs.move(
          moveTabIds['b'], {windowId: secondWindowId, index: 0},
          pass(function(tabB) {
            assertEq(0, tabB.index);
            chrome.tabs.move(moveTabIds['e'], {index: 2}, pass(function(tabE) {
                               assertEq(2, tabE.index);
                               chrome.tabs.move(
                                   moveTabIds['d'],
                                   {windowId: secondWindowId, index: 2},
                                   pass(function(tabD) {
                                     assertEq(2, tabD.index);
                                     checkMoveResults();
                                   }));
                             }));
          }));
    },

    function remove() {
      chrome.tabs.remove(moveTabIds['d'], pass(function() {
                           chrome.tabs.query(
                               {windowId: secondWindowId}, pass(function(tabs) {
                                 assertEq(2, tabs.length);
                                 assertEq(pageUrl('b'), tabs[0].url);
                                 assertTrue(newTabUrls.includes(tabs[1].url));
                               }));
                         }));
    },

    function moveMultipleTabs() {
      chrome.tabs.move(
          [moveTabIds['c'], moveTabIds['a']],
          {windowId: secondWindowId, index: 1}, pass(function(tabsA) {
            assertEq(2, tabsA.length);
            assertEq(secondWindowId, tabsA[0].windowId);
            assertEq(pageUrl('c'), tabsA[0].url);
            assertEq(1, tabsA[0].index);
            assertEq(secondWindowId, tabsA[1].windowId);
            assertEq(pageUrl('a'), tabsA[1].url);
            assertEq(2, tabsA[1].index);
            chrome.tabs.query({windowId: secondWindowId}, pass(function(tabsB) {
                                assertEq(4, tabsB.length);
                              }));
          }));
    },

    function removeMultipleTabs() {
      chrome.tabs.remove([moveTabIds['e'], moveTabIds['c']], pass(function() {
                           chrome.tabs.query(
                               {windowId: secondWindowId}, pass(function(tabs) {
                                 assertEq(3, tabs.length);
                                 assertEq(pageUrl('b'), tabs[0].url);
                                 assertEq(pageUrl('a'), tabs[1].url);
                                 assertTrue(newTabUrls.includes(tabs[2].url));
                               }));
                         }));
    },

    // Make sure we don't crash when the index is out of range.
    function moveToInvalidTab() {
      const expectedError = 'Error in invocation of tabs.move(' +
          '[integer|array] tabIds, object moveProperties, ' +
          'optional function callback): Error at parameter \'moveProperties\': ' +
          'Error at property \'index\': Value must be at least -1.';
      let caught = false;
      try {
        chrome.tabs.move(moveTabIds['b'], {index: -2}, function(tab) {
          chrome.test.fail('Moved a tab to an invalid index');
        });
      } catch (e) {
        assertEq(expectedError, e.message);
        caught = true;
      }
      assertTrue(caught);
      chrome.test.succeed();
    },

    // Check that attempting to move a non-existent tab returns an error.
    function moveNonExistentTab() {
      chrome.tabs.move(999999, {index: 0}, fail('No tab with id: 999999.'));
    },
  ]);
});
