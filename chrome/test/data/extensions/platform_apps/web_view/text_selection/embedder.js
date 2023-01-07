// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

var LOG = function(msg) { window.console.log(msg); };

embedder.setUp = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/text_selection' +
      '/guest.html';
  LOG('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    chrome.test.fail('No <webview> element created');
  }
  return webview;
};

// Tests begin.
function testSelection(webview) {
  var onLoadStop = function(e) {
    LOG('webview.onLoadStop');
    chrome.test.sendMessage('connected');
  };
  webview.addEventListener('loadstop', onLoadStop);
  webview.addEventListener('consolemessage', function(e) {
    if (e.message == 'GUEST_CONTEXTMENU') {
      chrome.test.sendMessage('MSG_CONTEXTMENU');
    }
  });
  webview.src = embedder.guestURL;
}

embedder.startTests = function startTests() {
   var webview = embedder.setUpGuest_();
   testSelection(webview);
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    embedder.startTests();
  });
};
