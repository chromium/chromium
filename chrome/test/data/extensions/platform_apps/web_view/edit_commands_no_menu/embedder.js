// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

embedder.setUp = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/edit_commands_no_menu' +
      '/guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
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

/** @private */
embedder.waitForResponseFromGuest_ =
    function(webview,
             testName,
             channelCreationCallback,
             expectedResponse,
             responseCallback) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'channel-created') {
      channelCreationCallback();
      // Schedule the 'connected' notification to the main test so that the
      // postMessage scheduled in channelCreationCallback() is sent first.
      setTimeout(function () {
        chrome.test.sendMessage('connected');
      }, 0);
      return;
    }
    if (response == 'testinput-focused') {
      setTimeout(function () {
        chrome.test.sendMessage('Focused');
      }, 0);
      return;
    }
    console.log('response: ' + response);
    var name = data[1];
    if ((response != expectedResponse) || (name != testName)) {
      return;
    }
    responseCallback();
    window.removeEventListener('message', onPostMessageReceived);
  };
  window.addEventListener('message', onPostMessageReceived);

  var onWebViewLoadStop = function(e) {
    // This creates a communication channel with the guest.
    webview.contentWindow.postMessage(
        JSON.stringify(['create-channel', testName]), '*');
    webview.removeEventListener('loadstop', onWebViewLoadStop);
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.setAttribute('src', embedder.guestURL);
};

// Tests begin.

embedder.tests.testStartOfLineWhenFocused =
    function testStartOfLineWhenFocused(webview) {
  embedder.waitForResponseFromGuest_(webview, 'testStartOfLineWhenFocused',
      function() {
    webview.focus();
    webview.contentWindow.postMessage(JSON.stringify(['end-of-line']), '*');
  }, 'caret-position-0', function() {
    chrome.test.sendMessage('StartOfLine');
  });
}

 embedder.startTests = function startTests() {
   var webview = embedder.setUpGuest_();
   embedder.tests.testStartOfLineWhenFocused(webview);
 };

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    embedder.startTests();
  });
};
