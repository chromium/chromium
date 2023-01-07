// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var startTest = function() {
  var webview = document.querySelector('webview');
  var onLoadStop = function(e) {
    webview.contentWindow.postMessage(JSON.stringify(['connect']),'*');
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });
  webview.partition = 'partition1';
  webview.src = 'guest.html';
};

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  LOG('data: ' + data);
  switch (data[0]) {
    case 'connected':
      chrome.test.sendMessage('WebViewTest.LAUNCHED');
      break;
    case 'installed-touch-handler':
    case 'uninstalled-touch-handler':
      chrome.test.sendMessage(data[0]);
      break;
  }
});

chrome.test.getConfig(function(config) {
  var guestURL = 'data:text/html,<html><body>foo</body></html>';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 10px; height: 10px; margin: 0; padding: 0;"' +
      '></webview>';
  startTest();
});
