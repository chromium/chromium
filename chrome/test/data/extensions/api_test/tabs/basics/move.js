// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;
var secondWindowId;
var moveTabIds = {};
var kChromeUINewTabURL = "chrome://newtab/";

var newTabUrls = [
  kChromeUINewTabURL,
  // The tab URL for the Local New Tab Page.
  'chrome-search://local-ntp/local-ntp.html',
];

// Check if callback object is same as the expected/actual behaviour.
function checkMoveResult(movedTab, expectedResult) {
  const { tabId, windowId, index, pinned } = expectedResult;
  assertEq(index, movedTab.index);
  assertEq(windowId, movedTab.windowId);
  assertEq(tabId, movedTab.id);
  assertEq(pinned, movedTab.pinned);
};

function pinTab(tabId) {
  return new Promise((resolve) => {
    chrome.tabs.update(tabId, { pinned: true }, (tab) => {
      chrome.test.assertNoLastError();
      resolve(tab);
    });
  });
};

function moveTab(tabId, toWindowId, toIndex) {
  return new Promise(resolve => {
    chrome.tabs.move(
      tabId,
      { windowId: toWindowId, index: toIndex },
      tab => {
        chrome.test.assertNoLastError();
        resolve(tab);
    });
  });
}

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
    var pages = [kChromeUINewTabURL, pageUrl('a'), pageUrl('b'),
                   pageUrl('c'), pageUrl('d'), pageUrl('e')];
    createWindow(pages, {}, pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        firstWindowId = winId;
        moveTabIds['a'] = tabIds[1];
        moveTabIds['b'] = tabIds[2];
        moveTabIds['c'] = tabIds[3];
        moveTabIds['d'] = tabIds[4];
        moveTabIds['e'] = tabIds[5];
        createWindow([kChromeUINewTabURL], {}, pass(function(winId, tabIds) {
          waitForAllTabs(pass(function() {
            secondWindowId = winId;
          }));
        }));
        chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
          assertEq(pages.length, tabs.length);
          assertTrue(newTabUrls.includes(tabs[0].url));
          for (var i = 1; i < tabs.length; i++) {
            assertEq(pages[i], tabs[i].url);
          }
        }));
      }));
    }));
  },

  function move() {
    // Check that the tab/window state is what we expect after doing moves.
    function checkMoveResults() {
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(4, tabs.length);
        assertTrue(newTabUrls.includes(tabs[0].url));
        assertEq(pageUrl("a"), tabs[1].url);
        assertEq(pageUrl("e"), tabs[2].url);
        assertEq(pageUrl("c"), tabs[3].url);

        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(3, tabs.length);
          assertEq(pageUrl("b"), tabs[0].url);
          assertTrue(newTabUrls.includes(tabs[1].url));
          assertEq(pageUrl("d"), tabs[2].url);
        }));
      }));
    }

    chrome.tabs.move(moveTabIds['b'], {"windowId": secondWindowId, "index": 0},
                     pass(function(tabB) {
      assertEq(0, tabB.index);
      chrome.tabs.move(moveTabIds['e'], {"index": 2},
                       pass(function(tabE) {
        assertEq(2, tabE.index);
        chrome.tabs.move(moveTabIds['d'], {"windowId": secondWindowId,
                         "index": 2}, pass(function(tabD) {
          assertEq(2, tabD.index);
          checkMoveResults();
        }));
      }));
    }));
  },

  function moveWithNegativeIndex() {
    // Check that the tab/window state is what we expect after doing moves.
    function checkMoveResults() {
      chrome.tabs.getAllInWindow(firstWindowId, pass(function(tabs) {
        assertEq(3, tabs.length);
        assertTrue(newTabUrls.includes(tabs[0].url));
        assertEq(pageUrl("a"), tabs[1].url);
        assertEq(pageUrl("c"), tabs[2].url);

        chrome.tabs.getAllInWindow(secondWindowId, pass(function(tabs) {
          assertEq(4, tabs.length);
          assertEq(pageUrl("b"), tabs[0].url);
          assertTrue(newTabUrls.includes(tabs[1].url));
          assertEq(pageUrl("d"), tabs[2].url);
          assertEq(pageUrl("e"), tabs[3].url);
        }));
      }));
    }

    // A -1 move index means move the tab to the end of the window.
    chrome.tabs.move(moveTabIds['e'], {"windowId": secondWindowId, "index": -1},
                     pass(function(tabE) {
        assertEq(3, tabE.index);
        checkMoveResults();
    }));
  },

  function remove() {
    chrome.tabs.remove(moveTabIds["d"], pass(function() {
      chrome.tabs.getAllInWindow(secondWindowId,
                                 pass(function(tabs) {
        assertEq(3, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertTrue(newTabUrls.includes(tabs[1].url));
        assertEq(pageUrl("e"), tabs[2].url);
      }));
    }));
  },

  function moveMultipleTabs() {
    chrome.tabs.move([moveTabIds['c'], moveTabIds['a']],
                     {"windowId": secondWindowId, "index": 1},
                     pass(function(tabsA) {
      assertEq(2, tabsA.length);
      assertEq(secondWindowId, tabsA[0].windowId);
      assertEq(pageUrl('c'), tabsA[0].url);
      assertEq(1, tabsA[0].index);
      assertEq(secondWindowId, tabsA[1].windowId);
      assertEq(pageUrl('a'), tabsA[1].url);
      assertEq(2, tabsA[1].index);
      chrome.tabs.query({"windowId": secondWindowId}, pass(function(tabsB) {
        assertEq(5, tabsB.length);
      }));
    }));
  },

  function removeMultipleTabs() {
    chrome.tabs.remove([moveTabIds['e'], moveTabIds['c']], pass(function() {
      chrome.tabs.query({"windowId": secondWindowId}, pass(function(tabs) {
        assertEq(3, tabs.length);
        assertEq(pageUrl("b"), tabs[0].url);
        assertEq(pageUrl("a"), tabs[1].url);
        assertTrue(newTabUrls.includes(tabs[2].url));
      }));
    }));
  },

  // Make sure we don't crash when the index is out of range.
  function moveToInvalidTab() {
    var expectedJsBindingsError =
        'Invalid value for argument 2. Property \'index\': ' +
        'Value must not be less than -1.';
    var expectedNativeBindingsError =
        'Error in invocation of tabs.move(' +
        '[integer|array] tabIds, object moveProperties, ' +
        'optional function callback): Error at parameter \'moveProperties\': ' +
        'Error at property \'index\': Value must be at least -1.';
    var caught = false;
    try {
      chrome.tabs.move(moveTabIds['b'], {"index": -2}, function(tab) {
        chrome.test.fail("Moved a tab to an invalid index");
      });
    } catch (e) {
      assertTrue(e.message == expectedJsBindingsError ||
                 e.message == expectedNativeBindingsError, e.message);
      caught = true;
    }
    assertTrue(caught);
    chrome.tabs.move(moveTabIds['b'], {"index": 10000}, pass(function(tabB) {
      assertEq(2, tabB.index);
    }));
  },

  // Check that attempting to move an empty list of tabs doesn't crash browser
  function moveEmptyTabList() {
    chrome.tabs.move([], {"index": 0}, fail("No tabs given."));
  },

  // Move a tab to the current window.
  function moveToCurrentWindow() {
    chrome.windows.getCurrent(pass(function(win) {
      var targetWin = win.id == firstWindowId ? secondWindowId : firstWindowId;
      chrome.tabs.query({"windowId": targetWin}, pass(function(tabs) {
        chrome.tabs.move(tabs[0].id,
                         {"windowId": chrome.windows.WINDOW_ID_CURRENT,
                          "index": 0},
                         pass(function(tab) {
          assertEq(win.id, tab.windowId);
        }));
      }));
    }));
  },

  // Test that the correct tab index is returned for cases where extensions
  // try to move tabs across the pinned, not-pinned boundary in the
  // same window.
  function moveTabWithConstrainedIndexToSameWindow() {
    // Create and pin tabs
    function prepareTest(callback) {
      const pageNames = ['tab-a', 'tab-b', 'tab-c', 'tab-d'];
      setupWindow(pageNames).then(([winId, tabIds]) => {
        Promise.all([
          pinTab(tabIds['tab-a']),
          pinTab(tabIds['tab-b'])
        ]).then(() => {
          callback(winId, tabIds);
        });
      });
    };

    prepareTest((winId, tabIds) => {
      // pinned -> non-pinned(desired index adjusted to the end of
      // pinned tabs)
      // Initial state: [tab-a(pinned), tab-b(pinned), tab-c, tab-d]
      moveTab(tabIds['tab-a'], winId, 3).then((movedTab) => {
        // After state: [tab-b(pinned), tab-a(pinned), tab-c, tab-d]
        checkMoveResult(movedTab, {
          tabId: tabIds['tab-a'],
          windowId: winId,
          index: 1,
          pinned: true
        });

        // non-pinned -> pinned(desired index adjusted to the start of
        // non-pinned tabs)
        return moveTab(tabIds['tab-d'], winId, 0);
      }).then((movedTab) => {
        // After state: [tab-b(pinned), tab-a(pinned), tab-d, tab-c]
        checkMoveResult(movedTab, {
          tabId: tabIds['tab-d'],
          windowId: winId,
          index: 2,
          pinned: false
        });

        // pinned -> -1 index(desired index adjusted to the end of
        // pinned tabs)
        return moveTab(tabIds['tab-b'], winId, -1);
      }).then((movedTab) => {
        // After state: [tab-a(pinned), tab-b(pinned), tab-d, tab-c]
        checkMoveResult(movedTab, {
          tabId: tabIds['tab-b'],
          windowId: winId,
          index: 1,
          pinned: true
        });
      })
      .then(chrome.test.succeed)
      .catch((error) => {
        // If the test has failed, ignore the exception and return.
        if (error === 'chrome.test.failure')
          return;

        // We received an unexpected exception, fail the test.
        // This will re-throw.
        chrome.test.fail(error.stack || error);
      }).catch(() => {});
    });
  },
  // Test that the correct tab index is returned for cases where extensions
  // try to move tabs across the pinned, not-pinned boundary in the
  // different window.
  function moveTabWithConstrainedIndexToDifferentWindow() {
    // Create and pin tabs
    async function prepareTest(callback) {
      const firstWindowPageNames = [
        'tab-a',
        'tab-b',
        'tab-c',
        'tab-d',
        'tab-e'
      ];
      const secondWindowPageNames = [
        'tab-aa',
        'tab-bb',
        'tab-cc',
        'tab-dd',
        'tab-ee'
      ];
      const [firstWindow, secondWindow] = await Promise.all([
        setupWindow(firstWindowPageNames),
        setupWindow(secondWindowPageNames)
      ]);
      const [firstWindowId, firstWindowTabIds] = firstWindow;
      const [secondWindowId, secondWindowTabIds] = secondWindow;
      await Promise.all([
        pinTab(firstWindowTabIds['tab-a']),
        pinTab(firstWindowTabIds['tab-b']),
        pinTab(secondWindowTabIds['tab-aa']),
        pinTab(secondWindowTabIds['tab-bb'])
      ]);
      callback(
        firstWindowId,
        firstWindowTabIds,
        secondWindowId,
        secondWindowTabIds
      );
    };

    prepareTest((
      firstWindowId,
      firstWindowTabIds,
      secondWindowId,
      secondWindowTabIds
    ) => {
      // pinned -> pinned(unpinned and desired index adjusted
      // to the start of non-pinned tabs)
      // Initial firstWindow state:
      //          [tab-a(pinned), tab-b(pinned), tab-c, tab-d, tab-e]
      //         secondWindow state:
      //          [tab-aa(pinned), tab-bb(pinned), tab-cc, tab-dd, tab-ee]
      moveTab(firstWindowTabIds['tab-a'], secondWindowId, 0)
        .then((movedTab) => {
          // After firstWindow state:
          //        [tab-b(pinned), tab-c, tab-d, tab-e]
          //       secondWindow state:
          //        [tab-aa(pinned), tab-bb(pinned), tab-a(NOW non-pinned),
          //         tab-cc, tab-dd, tab-ee]
          checkMoveResult(movedTab, {
            tabId: firstWindowTabIds['tab-a'],
            windowId: secondWindowId,
            index: 2,
            pinned: false
          });

          // pinned -> non-pinned(unpinned and moved to desired index)
          return moveTab(firstWindowTabIds['tab-b'], secondWindowId, 4);
      }).then((movedTab) => {
        // After firstWindow state:
        //        [tab-c, tab-d, tab-e]
        //       secondWindow state:
        //        [tab-aa(pinned), tab-bb(pinned), tab-a, tab-cc,
        //         tab-b(NOW non-pinned), tab-dd, tab-ee]
        checkMoveResult(movedTab, {
          tabId: firstWindowTabIds['tab-b'],
          windowId: secondWindowId,
          index: 4,
          pinned: false
        });

        // non-pinned -> pinned(desired index adjusted to the start of
        // non-pinned tabs)
        return moveTab(firstWindowTabIds['tab-c'], secondWindowId, 0);
      }).then((movedTab) => {
        // After firstWindow state:
        //        [tab-d, tab-e]
        //       secondWindow state:
        //        [tab-aa(pinned), tab-bb(pinned), tab-c,
        //         tab-a, tab-cc, tab-b, tab-dd, tab-ee]
        checkMoveResult(movedTab, {
          tabId: firstWindowTabIds['tab-c'],
          windowId: secondWindowId,
          index: 2,
          pinned: false
        });

        // pinned -> -1 index(unpinned and desired index adjusted to
        // the end of non-pinned tabs)
        return moveTab(secondWindowTabIds['tab-aa'], firstWindowId, -1);
      }).then((movedTab) => {
        // After firstWindow state:
        //        [tab-d, tab-e, tab-aa(Now non-pinned)]
        //       secondWindow state:
        //        [tab-bb(pinned), tab-c, tab-a, tab-cc, tab-b,
        //         tab-dd, tab-ee]
        checkMoveResult(movedTab, {
          tabId: secondWindowTabIds['tab-aa'],
          windowId: firstWindowId,
          index: 2,
          pinned: false
        });
      })
      .then(chrome.test.succeed)
      .catch((error) => {
        // If the test has failed, ignore the exception and return.
        if (error === 'chrome.test.failure')
          return;

        // We received an unexpected exception, fail the test.
        // This will re-throw.
        chrome.test.fail(error.stack || error);
      }).catch(() => {});
    });
  }
]);
