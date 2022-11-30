// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNoLastError(message) {
  if (chrome.runtime.lastError) {
    message += ': ' + chrome.runtime.lastError.message;
  }
  chrome.test.assertFalse(!!chrome.runtime.lastError, message);
}

function executeWindowTest(testBody) {
  chrome.app.window.create(
      'appwindow.html', {}, function(appWindow) {
        assertNoLastError('window.create');
        appWindow.contentWindow.onload = function() {
          testBody(appWindow.contentWindow);
        };
      });
}

function createWebview(doc, onWebviewLoaded) {
  function onLoadStop(e) {
    e.target.removeEventListener('loadstop', onLoadStop);
    onWebviewLoaded();
  }
  var webview = doc.createElement('webview');
  webview.src = 'data:text/plain, test';
  webview.addEventListener('loadstop', onLoadStop);
  doc.body.appendChild(webview);
}

// This test verifies that it is possible to start logging again after it has
// been stopped.
function testStartStopStart() {
  executeWindowTest(function(win) {
    createWebview(win.document, function() {
      win.attemptStartStopLogging(
        function() {  // Do it again.
          win.attemptStartStopLogging(
            chrome.test.succeed,
            chrome.test.fail);
        },
        chrome.test.fail);
    });
  });
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    testStartStopStart,
  ]);
});
