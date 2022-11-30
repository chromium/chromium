// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};

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
    console.log('Received response "' + response + '" from the guest content.');
    if (response == 'connected') {
      channelCreationCallback(webview);
      chrome.test.sendMessage('connected');
      return;
    }
    if (response == 'selected-all') {
      chrome.test.sendMessage('selected-all');
      return;
    }
    if (response != expectedResponse) {
      return;
    }
    responseCallback();
  };
  window.addEventListener('message', onPostMessageReceived);

  var onWebViewLoadStop = function(e) {
    webview.executeScript(
      {file: 'inject_edit_commands.js'},
      function(results) {
        console.log('Injected script into webview.');
        // Establish a communication channel with the webview1's guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
    webview.removeEventListener('loadstop', onWebViewLoadStop);
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);

  webview.setAttribute('src', 'about:blank');
};

// Tests begin.

// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

embedder.testEditCommands_ = function(testName,
                               channelCreationCallback,
                               expectedResponse,
                               responseCallback) {
  var webview = embedder.setUpGuest_();

  embedder.waitForResponseFromGuest_(webview,
                                     testName,
                                     channelCreationCallback,
                                     expectedResponse,
                                     responseCallback);
};

embedder.tests.testEditCommandsWhenFocused =
    function testEditCommandsWhenFocused() {
  embedder.testEditCommands_('testEditCommandsWhenFocused', function(webview) {
    // Focus the <webview> and select all the text when a communicaton channel
    // has been established.
    webview.focus();
    webview.contentWindow.postMessage(JSON.stringify(['select-all']), '*');
  }, 'copy', function() {
    chrome.test.sendMessage('copy');
  });
 }

 embedder.startTests = function startTests() {
  embedder.tests.testEditCommandsWhenFocused();
 };

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.startTests();
  });
};
