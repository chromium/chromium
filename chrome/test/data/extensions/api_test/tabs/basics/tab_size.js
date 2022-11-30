// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstTabId;
var firstTab;

chrome.test.runTests([
  function setupWindow() {
    createWindow([pageUrl("a")], {}, pass(function(winId, tabIds) {
      firstTabId = tabIds[0];
      chrome.tabs.get(firstTabId, pass(function(tab) {
        assertTrue(tab.width > 0);
        assertTrue(tab.height > 0);
        firstTab = tab;
      }));
    }));
  },

  function sizeAfterDuplicatingTab() {
    chrome.tabs.duplicate(firstTabId, pass(function(tab) {
      assertEq(firstTab.width, tab.width);
      assertEq(firstTab.height, tab.height);
    }));
  }
]);
