// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.getViews.
// browser_tests.exe --gtest_filter=ExtensionApiTest.GetViews

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

// We need to remember the popupWindowId to be able to find it later.
var popupWindowId = 0;

// Updated and used later to check getViews() tabId tag for the options tab.
var optionsTabId = 0;

// This function is called by the popup during the test run.
function popupCallback() {
  // We have now added a popup so the total count goes up one.
  assertEq(2, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({windowId: popupWindowId}).length);
  chrome.tabs.create({url: chrome.runtime.getURL("options.html")},
                     function(tab) {
    optionsTabId = tab.id;
  });
}

function optionsPageCallback() {
  assertEq(3, chrome.extension.getViews().length);
  assertEq(1, chrome.extension.getViews({windowId: popupWindowId}).length);
  assertEq(
      2, chrome.extension.getViews({type: "tab", windowId: window.id}).length);

  chrome.tabs.query({windowId: window.id}, function(tabs) {
    // Assert 3 to account for new tab page, web popup, & new tab options.html.
    assertEq(3, tabs.length);
    // getViews() only returns views that run in the extension process, so
    // this should only include the popup and options.html pages.
    assertEq(2, chrome.extension.getViews({type: "tab"}).length);

    for (var i = 0; i < tabs.length; i++) {
      if (tabs[i].windowId == popupWindowId) {
        assertEq(1, chrome.extension.getViews({tabId: tabs[i].id}).length);
        // Test tabId tag with other parameters.
        assertEq(1, chrome.extension.getViews({windowId: popupWindowId,
                                               tabId: tabs[i].id}).length);
        assertEq(1, chrome.extension.getViews({type: "tab",
                                               tabId: tabs[i].id}).length);
      } else if (tabs[i].id == optionsTabId) {
        assertEq(1, chrome.extension.getViews({tabId: tabs[i].id}).length);
      }
    }
  });

  chrome.test.notifyPass();
}

var tests = [
  function getViews() {
    assertTrue(typeof(chrome.extension.getBackgroundPage()) != "undefined");
    assertEq(1, chrome.extension.getViews().length);
    assertEq(0, chrome.extension.getViews({type: "tab"}).length);
    assertEq(0, chrome.extension.getViews({type: "popup"}).length);

    chrome.windows.getAll({populate: true}, function(windows) {
      assertEq(1, windows.length);

      // Create a popup window.

      // TODO (catmullings): Fix potential race condition when/if
      // popupCallback() is called before popupWindowId is set below
      chrome.windows.create({url: chrome.runtime.getURL("popup.html"),
                             type: "popup"}, function(window) {
        assertTrue(window.id > 0);
        popupWindowId = window.id;
        // The popup will call back to us through popupCallback (above).
      });
    });
  }
];

chrome.test.runTests(tests);
