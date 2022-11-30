// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var failTest = function() {
  chrome.test.sendMessage('WebViewTest.FAILURE');
};

var startTest = function() {
  chrome.test.sendMessage('WebViewTest.LAUNCHED');
};

window.onAppCommand = function(command) {
  LOG('onAppCommand: ' + command);
  switch (command) {
    case 'create-guest':
      var webview = document.createElement('webview');
      webview.src = 'data:text/html,<body>Guest</body>';
      document.body.appendChild(webview);
      chrome.test.sendMessage('WebViewTest.PASSED');
      break;
    default:
      failTest();
      break;
  }
};

window.onload = startTest;
