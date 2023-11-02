// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test verifies that the <webview> API is undefined if the webview
// permission is not specified in the manifest file.
function testAPIMethodExistence() {
  // See public-facing API functions in web_view_api_methods.js
  var WEB_VIEW_API_METHODS = [
    'addContentScripts',
    'back',
    'canGoBack',
    'canGoForward',
    'captureVisibleRegion',
    'clearData',
    'executeScript',
    'find',
    'forward',
    'getAudioState',
    'getProcessId',
    'getUserAgent',
    'getZoom',
    'getZoomMode',
    'go',
    'insertCSS',
    'isAudioMuted',
    'isSpatialNavigationEnabled',
    'isUserAgentOverridden',
    'loadDataWithBaseUrl',
    'print',
    'removeContentScripts',
    'reload',
    'setAudioMuted',
    'setSpatialNavigationEnabled',
    'setUserAgentOverride',
    'setZoom',
    'setZoomMode',
    'stop',
    'stopFinding',
    'terminate'
  ];

  var webview = document.createElement('webview');
  for (var methodName of WEB_VIEW_API_METHODS) {
    chrome.test.assertEq(
        'undefined', typeof webview[methodName],
        'Method should not be defined: ' + methodName);
  }

  // Check contentWindow.
  chrome.test.assertEq('undefined', typeof webview.contentWindow);
  chrome.test.succeed();
}

chrome.test.runTests([
  testAPIMethodExistence
]);
