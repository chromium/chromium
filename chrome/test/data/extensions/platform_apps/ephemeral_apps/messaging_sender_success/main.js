// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var targetAppId = 'idjoelgddomapgkhmbcoejocojmpjnme';

// Send a message to the ephemeral app and expect a response.
function testSendMessage() {
  chrome.runtime.sendMessage(targetAppId, 'hello', null, function(response) {
    if (response == 'ack_message')
      chrome.test.succeed();
    else
      chrome.test.fail();
  });
}

// Open a port to the ephemeral app, send a message and expect a response.
function testConnect() {
  var port = chrome.runtime.connect(targetAppId);
  var gotResponse = false;

  port.onMessage.addListener(function(response) {
    if (response == 'ack_connect_message') {
      chrome.test.succeed();
      gotResponse = true;
    } else {
      chrome.test.fail();
    }

    port.disconnect();
  });

  port.onDisconnect.addListener(function() {
    if (gotResponse)
      chrome.test.succeed();
    else
      chrome.test.fail();
  });

  port.postMessage('hello');
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched');

  chrome.test.runTests([
    testSendMessage,
    testConnect
  ]);

});
