// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kPortErrorMessage =
    'Could not establish connection. Receiving end does not exist.';

// onMessage / onConnect in the same frame cannot be triggered by sendMessage or
// connect, so both attempts to send a message should fail with an error.

chrome.runtime.onMessage.addListener(function(msg, sender, sendResponse) {
  chrome.test.fail('onMessage should not be triggered. Received: ' + msg);
});

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.fail('onConnect should not be triggered. Port: ' + port.name);
});

chrome.test.runTests([
  function sendMessageExpectingNoAnswer() {
    chrome.runtime.sendMessage('hello without callback');
    // The timer is here to try and get the test failure in the correct test
    // case (namely "sendMessageExpectingNoAnswer"). If the timer is too short,
    // but the test fails, then onMessage will still print an error that shows
    // which test fails, but the test runner will think that it is running in
    // the next test, and attribute the failure incorrectly.
    setTimeout(chrome.test.callbackPass(), 100);
  },

  function sendMessageExpectingNoAnswerWithCallback() {
    chrome.runtime.sendMessage('hello with callback',
        chrome.test.callbackFail(kPortErrorMessage));
  },

  function connectAndDisconnect() {
    chrome.runtime.connect({ name: 'The First Port'}).disconnect();
    // Like sendMessageExpectingNoAnswer; onConnect should not be triggered.
    setTimeout(chrome.test.callbackPass(), 100);
  },

  function connectExpectDisconnect() {
    chrome.runtime.connect({ name: 'The Last Port'}).onDisconnect.addListener(
        chrome.test.callbackFail(kPortErrorMessage));
  },

  // Regression test for crbug.com/597698
  function sendMessageNoCallback() {
    var f = document.createElement('iframe');
    var onMessageInFrame = chrome.test.callbackPass(function(msg) {
      f.remove();
      chrome.test.assertEq('sendMessage without callback', msg);
    });
    f.onload = function() {
      f.contentWindow.chrome.runtime.onMessage.addListener(onMessageInFrame);
      chrome.runtime.sendMessage('sendMessage without callback');
    };

    // The exact file is not important, as long as it is an extension page, so
    // that the extension APIs become available (about:blank would not work).
    f.src = 'manifest.json';
    document.body.appendChild(f);
  },

  // Regression test for crbug.com/597698
  function connectAndDisconnectInIframe() {
    var gotMessage = chrome.test.callbackAdded();
    var gotDisconnect = chrome.test.callbackAdded();

    var senderPort;
    var f = document.createElement('iframe');
    f.onload = function() {
      f.contentWindow.chrome.runtime.onConnect.addListener(function(port) {
        chrome.test.assertEq('port with active frame', port.name);
        chrome.test.assertEq(null, senderPort, 'onConnect should be async');
        var didCallOnMessage = false;
        port.onMessage.addListener(function(msg) {
          chrome.test.assertEq(false, didCallOnMessage);
          didCallOnMessage = true;
          chrome.test.assertEq('fire and forget', msg);
          gotMessage();
        });
        port.onDisconnect.addListener(function() {
          f.remove();
          gotDisconnect();
        });
      });

      senderPort = chrome.runtime.connect({ name: 'port with active frame' });
      senderPort.postMessage('fire and forget');
      senderPort.disconnect();
      senderPort = null;
    };

    // The exact file is not important, as long as it is an extension page, so
    // that the extension APIs become available (about:blank would not work).
    f.src = 'manifest.json';
    document.body.appendChild(f);
  },
]);
