// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.guestURL = '';

window.runTest = function(testName) {
  if (testName == 'testLoadWebviewInsideIframe') {
    testLoadWebviewInsideIframe();
  } else {
    window.console.log('Incorrect testName: ' + testName);
    chrome.test.sendMessage('TEST_FAILED');
  }
}

function testLoadWebviewInsideIframe() {
  var iframe = document.querySelector('iframe');
  var webview = iframe.contentDocument.querySelector('webview');

  if (webview.contentWindow === undefined) {
    window.console.log('The webview was not initialized.');
    chrome.test.sendMessage('TEST_FAILED');
    return;
  }

  webview.addEventListener('loadstop', function() {
    window.addEventListener('message', function(e) {
      if (e.data == 'TEST_PASSED') {
        chrome.test.sendMessage('TEST_PASSED');
      } else {
        chrome.test.sendMessage('TEST_FAILED');
      }
    });
    webview.contentWindow.postMessage('TEST_START', '*');
  });

  webview.src = embedder.guestURL;
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.guestURL =
        'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/web_view/load_webview_inside_iframe/' +
        'guest.html';
    chrome.test.sendMessage('Launched');
  });
};
