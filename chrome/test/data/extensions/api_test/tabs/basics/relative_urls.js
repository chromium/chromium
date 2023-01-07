// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;
var relativePageLoaded;

chrome.test.runTests([
  function setupRelativeUrlTests() {
    createWindow(["about:blank"], {}, pass(function(winId, tabIds) {
      firstWindowId = winId;
    }));
  },

  function relativeUrlTestsTabsCreate() {
    // Will be called from relative.html
    relativePageLoaded = chrome.test.callbackAdded();
    var createCompleted = chrome.test.callbackAdded();

    chrome.tabs.create({windowId: firstWindowId, url: 'relative.html'},
      function(tab){
        testTabId = tab.id;
        createCompleted();
      }
    );
  },

  function relativeUrlTestsTabsUpdate() {
    // Will be called from relative.html
    relativePageLoaded = chrome.test.callbackAdded();

    chrome.tabs.update(testTabId, {url: pageUrl("a")}, function(tab) {
      chrome.test.assertEq(pageUrl("a"), tab.pendingUrl);
      chrome.tabs.update(tab.id, {url: "relative.html"}, function(tab) {
      });
    });
  },

  function relativeUrlTestsWindowCreate() {
    // Will be called from relative.html
    relativePageLoaded = chrome.test.callbackAdded();

    chrome.windows.create({url: "relative.html"});
  }

]);
