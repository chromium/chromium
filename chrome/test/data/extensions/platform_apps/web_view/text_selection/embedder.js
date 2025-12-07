// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

var LOG = function(msg) { window.console.log(msg); };

embedder.setUpBaseGuestURL = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
};

/** @private */
embedder.setUpGuest_ = function(guestURL) {
  embedder.guestURL = embedder.baseGuestURL + guestURL;
  LOG('Guest url is: ' + embedder.guestURL);

  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 200px; height: 200px;"></webview>';
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

embedder.startTests = function startTests(guestURL) {
   var webview = embedder.setUpGuest_(guestURL);
   testSelection(webview);
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUpBaseGuestURL(config);
    chrome.test.sendMessage('launched');
  });
};

window.onAppMessage = function(guestURL) {
  embedder.startTests(guestURL);
}
