// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId;
var otherTabId;
var firstWindowId;
var secondWindowId;

chrome.test.runTests([
  function init() {
    chrome.tabs.getSelected(null, pass(function(tab) {
      testTabId = tab.id;
      firstWindowId = tab.windowId;
    }));
  },

  function tabsOnCreated() {
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      assertEq(pageUrl("f"), tab.pendingUrl || tab.url);
      otherTabId = tab.id;
      assertEq(true, tab.selected);
    });

    chrome.tabs.create({"windowId": firstWindowId, "url": pageUrl("f"),
                        "selected": true}, pass(function(tab) {}));
  },

  function tabsOnUpdatedIgnoreTabArg() {
    // A third argument was added to the onUpdated event callback.
    // Test that an event handler which ignores this argument works.
    var onUpdatedCompleted = chrome.test.listenForever(chrome.tabs.onUpdated,
      function(tabid, changeInfo) {
        if (tabid == otherTabId && changeInfo.status == "complete") {
          onUpdatedCompleted();
        }
      }
    );

    chrome.tabs.update(otherTabId, {"url": pageUrl("f")}, pass());
  },

  function tabsOnUpdated() {
    var onUpdatedCompleted = chrome.test.listenForever(
      chrome.tabs.onUpdated,
      function(tabid, changeInfo, tab) {
        // |tab| contains the id of the tab it describes.
        // Test that |tabid| matches this id.
        assertEq(tabid, tab.id);

        // If |changeInfo| has a status property, than
        // it should match the status of the tab in |tab|.
        if (changeInfo.status) {
          assertEq(changeInfo.status, tab.status);
        }

        if (tabid == otherTabId && changeInfo.status == "complete") {
          onUpdatedCompleted();
        }
      }
    );

    chrome.tabs.update(otherTabId, {"url": pageUrl("f")}, pass());
  },

  function tabsOnMoved() {
    chrome.test.listenOnce(chrome.tabs.onMoved, function(tabid, info) {
      assertEq(otherTabId, tabid);
    });

    chrome.tabs.move(otherTabId, {"index": 0}, pass());
  },

  function tabsOnSelectionChanged() {
    // Note: tabs.onSelectionChanged is deprecated.
    chrome.test.listenOnce(chrome.tabs.onSelectionChanged,
      function(tabid, info) {
        assertEq(testTabId, tabid);
        assertEq(firstWindowId, info.windowId);
      }
    );

    chrome.tabs.update(testTabId, {"selected": true}, pass());
  },

  function tabsOnActiveChanged() {
    // Note: tabs.onActiveChanged is deprecated.
    chrome.test.listenOnce(chrome.tabs.onActiveChanged,
      function(tabid, info) {
        assertEq(otherTabId, tabid);
        assertEq(firstWindowId, info.windowId);
      }
    );

    chrome.tabs.update(otherTabId, {"active": true}, pass());
  },

  function tabsOnActivated() {
    chrome.test.listenOnce(chrome.tabs.onActivated,
      function(info) {
        assertEq(testTabId, info.tabId);
        assertEq(firstWindowId, info.windowId);
      }
    );

    chrome.tabs.update(testTabId, {"active": true}, pass());
  },

  function setupTabsOnAttachDetach() {
    createWindow([""], {}, pass(function(winId, tabIds) {
      secondWindowId = winId;
    }));
  },

  function tabsOnAttached() {
    function moveAndListen(tabId, properties, callback) {
      chrome.test.listenOnce(chrome.tabs.onAttached,
                             function(testTabId, info) {
        // Ensure notification is correct.
        assertEq(testTabId, tabId);
        assertEq(properties.windowId, info.newWindowId);
        assertEq(properties.index, info.newPosition);
        if (callback)
          callback();
      });
      chrome.tabs.move(tabId, properties);
    };

    // Move tab to second window, then back to first.
    // The original tab/window configuration should be restored.
    // tabsOnDetached() depends on it.
    moveAndListen(testTabId, {"windowId": secondWindowId, "index": 0},
                  pass(function() {
      moveAndListen(testTabId, {"windowId": firstWindowId, "index": 1});
    }));
  },

  function tabsOnDetached() {
    function moveAndListen(tabId, oldWindowId, oldIndex, properties,
                                 callback) {
      chrome.test.listenOnce(chrome.tabs.onDetached,
                             function(detachedTabId, info) {
        // Ensure notification is correct.
        assertEq(detachedTabId, tabId);
        assertEq(oldWindowId, info.oldWindowId);
        assertEq(oldIndex, info.oldPosition);
        if (callback)
          callback();
      });
      chrome.tabs.move(tabId, properties);
    };

    // Move tab to second window, then back to first.
    moveAndListen(testTabId, firstWindowId, 1,
                  {"windowId": secondWindowId, "index": 0}, pass(function() {
      moveAndListen(testTabId, secondWindowId, 0,
                    {"windowId": firstWindowId, "index": 1});
                  }));
  },

  function tabsOnZoomChange() {
    chrome.tabs.setZoom(testTabId, 1, function() {
      chrome.test.listenOnce(
          chrome.tabs.onZoomChange,
          function(zoomChangeInfo) {
            assertEq(testTabId, zoomChangeInfo.tabId);
            assertEq(1, zoomChangeInfo.oldZoomFactor);
            assertEq(3.14159, +zoomChangeInfo.newZoomFactor.toFixed(5));
            assertEq("automatic", zoomChangeInfo.zoomSettings.mode);
            assertEq("per-origin", zoomChangeInfo.zoomSettings.scope);
          });

      chrome.tabs.setZoom(testTabId, 3.14159);
    });
  },

  function windowsOnCreated() {
    chrome.test.listenOnce(chrome.windows.onCreated, function(window) {
      assertTrue(window.width > 0);
      assertTrue(window.height > 0);
      assertEq("normal", window.type);
      assertTrue(!window.incognito);
      windowEventsWindow = window;
      chrome.tabs.getAllInWindow(window.id, pass(function(tabs) {
        assertEq(pageUrl("a"), tabs[0].pendingUrl || tabs[0].url);
      }));
    });

    chrome.windows.create({"url": pageUrl("a")}, pass(function(tab) {}));
  },

  /*
  This test doesn't work on mac because the Chromium app never gets
  brought to the front. See: crbug.com/60963.
  It also doesn't work on Chrome OS for unknown reasons.
  It also times out on the full XP builder for unknown reasons.
  See: crbug.com/61035.

  function windowsOnFocusChanged() {
    chrome.windows.getCurrent(pass(function(windowA) {
      chrome.windows.create({}, pass(function(windowB) {
        chrome.windows.update(windowA.id, {focused: true}, pass(function() {
          chrome.windows.update(windowB.id, {focused: true}, pass(function() {
            chrome.test.listenOnce(chrome.windows.onFocusChanged,
                                   function(changedWindowId) {
              assertEq(windowEventsWindow.id, changedWindowId);
            });
            chrome.windows.remove(windowB.id);
          }));
        }));
      }));
    }));
  }
  */
]);
