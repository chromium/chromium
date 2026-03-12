// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appName = 'com.google.chrome.test.echo';
var inServiceWorker = 'ServiceWorkerGlobalScope' in self;
var extensionUrl = chrome.runtime.getURL('/');

function checkMessageUrl(url) {
  if (inServiceWorker) {
    chrome.test.assertEq(extensionUrl, url);
  } else {
    chrome.test.assertEq(window.location.origin + '/', url);
  }
}

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function connect() {
      var messagesToSend =
          [{'text': 'foo'}, {'text': 'bar', 'funCount': 9001}, {}];
      var currentMessage = 0;

      port = chrome.runtime.connectNative(appName);
      port.postMessage(messagesToSend[currentMessage]);

      port.onMessage.addListener(function(message) {
        chrome.test.assertEq(currentMessage + 1, message.id);
        chrome.test.assertEq(messagesToSend[currentMessage], message.echo);
        checkMessageUrl(message.caller_url);
        currentMessage++;

        if (currentMessage == messagesToSend.length) {
          chrome.test.succeed();
        } else {
          port.postMessage(messagesToSend[currentMessage]);
        }
      });
    },

    // Verify that the case when host stops itself is handled properly.
    function stopHost() {
      port = chrome.runtime.connectNative(appName);

      port.onDisconnect.addListener(
          chrome.test.callback(function() {}, 'Native host has exited.'));

      // Send first message that should stop the host.
      port.postMessage({'stopHostTest': true});
    }
  ]);
});
