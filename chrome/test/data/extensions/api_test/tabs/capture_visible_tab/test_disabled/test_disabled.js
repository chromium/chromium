// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.tabs.captureVisibleTab(), screenshot disabling policy.
// browser_tests.exe --gtest_filter=ExtensionApiTest.CaptureVisibleDisabled

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;

var kWindowRect = {
  'width': 400,
  'height': 400
};

const scriptUrl =
      '_test_resources/api_test/tabs/capture_visible_tab/common/tabs_util.js';

let loadScript = chrome.test.loadScript(scriptUrl);
loadScript.then(() => {chrome.test.getConfig((config) => {
  const kError = 'Taking screenshots has been disabled';
  const kUrl = `http://localhost:${config.testServer.port}/simple.html`;
  chrome.test.runTests([
    function captureVisibleDisabled() {
      createWindow([kUrl], kWindowRect, pass(function(winId, tabIds) {
        waitForAllTabs(pass(function() {
          chrome.tabs.query({active: true, windowId: winId},
                            pass(function(tabs) {
            assertEq('complete', tabs[0].status);
            chrome.tabs.captureVisibleTab(winId, fail(kError));
          }));
        }));
      }));
    },

    function captureVisibleDisabledInNullWindow() {
      chrome.tabs.create({url: kUrl}, pass(() => {
        waitForAllTabs(pass(() => {
          chrome.tabs.captureVisibleTab(null, fail(kError));
        }));
      }));
    },

    function captureVisibleDisabledInCurrentWindow() {
      chrome.tabs.captureVisibleTab(chrome.windows.WINDOW_ID_CURRENT,
                                    fail(kError));
    }
  ])});
});
