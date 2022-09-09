// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var targetAppId = 'idjoelgddomapgkhmbcoejocojmpjnme';

// Send a message to the ephemeral app and expect no response.
function testSendMessage() {
  chrome.runtime.sendMessage(targetAppId, 'hello', null, function(response) {
    if (response)
      chrome.test.fail();
    else
      chrome.test.succeed();
  });
}

// Open a port to the ephemeral app and expect this to fail.
function testConnect() {
  var port = chrome.runtime.connect(targetAppId);
  var gotResponse = false;

  port.onMessage.addListener(function(response) {
    gotResponse = true;
    chrome.test.fail();
    port.disconnect();
  });

  port.onDisconnect.addListener(function() {
    if (gotResponse)
      chrome.test.fail();
    else
      chrome.test.succeed();
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
