// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const LOG = function(msg) {
  window.console.log(msg);
};

const failTest = function() {
  chrome.test.sendMessage('WebViewTest.FAILURE');
};

const startTest = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 10px; height: 10px; margin: 0; padding: 0;"' +
      '></webview>';

  const webview = document.querySelector('webview');
  const onLoadStop = function(e) {
    chrome.test.sendMessage('LAUNCHED');
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
