// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };

// window.* exported functions begin.
window.runTest = function(testName) {
  chrome.test.sendMessage('TEST_PASSED');
};
// window.* exported functions end.

function setUpTest(messageCallback) {
  var guestUrl = 'data:text/html,<html><body>guest</body></html>';
  var webview = document.createElement('webview');

  var onLoadStop = function(e) {
    LOG('webview has loaded.');
    messageCallback(webview);
  };
  webview.addEventListener('loadstop', onLoadStop);

  webview.setAttribute('src', guestUrl);
  document.body.appendChild(webview);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    setUpTest(function(webview) {
      LOG('Guest load completed.');
      chrome.test.sendMessage('WebViewTest.LAUNCHED');
      chrome.test.sendMessage('connected');
      chrome.test.sendMessage('Launched');
    });
  });
};
