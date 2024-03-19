// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), capturing JPEG images.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleFile

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;

var kWindowRect = {
  'width': 400,
  'height': 400
};

var fail_url = "file:///nosuch.html";

const scriptUrl =
    '_test_resources/api_test/tabs/capture_visible_tab/common/tabs_util.js';

let loadScript = chrome.test.loadScript(scriptUrl);
loadScript.then(() => {chrome.test.runTests([
  // Check that test infrastructure launched us with permissions.
  function checkAllowedAccess() {
    chrome.extension.isAllowedFileSchemeAccess(pass(function(hasAccess) {
       assertTrue(hasAccess);
    }));
  },

  // Check that we get image data back (but not the contents, which are
  // platform-specific)
  function captureVisibleFile() {
    createWindow([fail_url], kWindowRect, pass(function(winId, tabIds) {
      waitForAllTabs(pass(function() {
        chrome.tabs.query({active: true, windowId: winId}, pass(function(tabs) {
          assertEq('complete', tabs[0].status);
          chrome.tabs.captureVisibleTab(winId, pass(function(imgDataUrl) {
            // The URL should be a data URL with has a JPEG mime type.
            assertEq('string', typeof(imgDataUrl));
            assertEq('data:image/jpeg;base64,', imgDataUrl.substr(0,23));
          }));
        }));
      }));
    }));
  },

  function captureVisibleFileInNullWindow() {
    chrome.tabs.captureVisibleTab(null, pass(function(imgDataUrl) {
      // The URL should be a data URL with has a JPEG mime type.
      assertEq('string', typeof(imgDataUrl));
      assertEq('data:image/jpeg;base64,', imgDataUrl.substr(0,23));
    }));
  },

  function captureVisibleFileInCurrentWindow() {
    chrome.tabs.captureVisibleTab(chrome.windows.WINDOW_ID_CURRENT,
                                  pass(function(imgDataUrl) {
      // The URL should be a data URL with has a JPEG mime type.
      assertEq('string', typeof(imgDataUrl));
      assertEq('data:image/jpeg;base64,', imgDataUrl.substr(0,23));
    }));
  }
])});
