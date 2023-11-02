// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var webview = null;

var LOG = function(msg) {
  window.console.log(msg);
};

var startTest = function(guestURL) {
  window.onmessage = onMessage;
  chrome.test.sendMessage('guest-loaded');
  webview = document.getElementById('webview');
  webview.onloadstop = onWebviewLoaded;
  webview.onconsolemessage = function(e) { LOG('[Guest]: ' + e.message); };

  webview.setAttribute('src', guestURL);
};

var onWebviewLoaded = function(event) {
  LOG('onWebviewLoaded');
  webview.contentWindow.postMessage(JSON.stringify(['ping']), '*');
};

var onMessage = function(e) {
  var data = JSON.parse(e.data);
  if (data.length != 1 || data[0] !== 'pong') {
    LOG('Unexpected data received from webview.');
    chrome.test.fail();
  } else if (e.source != webview.contentWindow) {
    LOG('wrong event.source in postMessage');
    chrome.test.fail();
  } else {
    chrome.test.succeed();
  }
};

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
      function postMessage() {
        var guestURL = 'http://localhost:' + config.testServer.port +
            '/extensions/platform_apps/web_view/post_message/basic/guest.html';
        LOG('guestURL: ' + guestURL);
        document.querySelector('#webview-tag-container').innerHTML =
            '<webview id="webview"></webview>';
        startTest(guestURL);
      }]);
});
