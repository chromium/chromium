// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';
embedder.iframeURL = '';

/** @private */
embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/geolocation' +
      '/cancel_request/pages/guest.html';
  embedder.iframeURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/geolocation' +
      '/cancel_request/pages/iframe.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      '></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    chrome.test.fail();
  }
  return webview;
};

/** @private */
embedder.setUpLoadStop_ = function(webview, testName, opt_iframeURL) {
  var loadstopCalled = false;
  var onWebviewLoadStop = function(e) {
    if (loadstopCalled) {
      return;
    }
    loadstopCalled = true;
    // Send post message to <webview> when it's ready to receive them.
    var msgArray = [
      'test-cancel-geolocation',
      '' + testName,
      embedder.iframeURL
    ];
    webview.contentWindow.postMessage(JSON.stringify(msgArray), '*');
  };
  webview.addEventListener('loadstop', onWebviewLoadStop);
};


/** @private */
embedder.registerAndWaitForPostMessage_ = function(
    webview, expectedData) {
  var testName = expectedData[0];
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName) {
      chrome.test.assertEq(expectedData, data);
      chrome.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  chrome.test.assertEq('geolocation', e.permission);
  chrome.test.assertTrue(!!e.url);
  chrome.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);
};

var g_requestObject;

// Tests begin.

// Tests CancelGeolocationPermission code path.
function testCancelGeolocationInIFrame() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.preventDefault();
    // keep a reference to request object so the permission keeps hanging.
    g_requestObject = e.request;
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test1');
  embedder.registerAndWaitForPostMessage_(
      webview, ['test1', 'PASSED']);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.runTests([
      testCancelGeolocationInIFrame,
    ]);
  });
};
