// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var failTest = function() {
  chrome.test.sendMessage('WebViewTest.FAILURE');
};

var startTest = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 10px; height: 10px; margin: 0; padding: 0;"' +
      '></webview>';

  var webview = document.querySelector('webview');
  var onLoadStop = function(e) {
    chrome.test.sendMessage('WebViewTest.LAUNCHED');
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = 'data:text/html,<body>Guest</body>';
};

window.onAppCommand = function(command) {
  LOG('onAppCommand: ' + command);
  switch (command) {
    case 'hide-guest':
      document.querySelector('webview').style.display = 'none';
      break;
    case 'hide-embedder':
      document.body.style.display = 'none';
      break;
    default:
      failTest();
      break;
  }
};

window.onload = startTest;
