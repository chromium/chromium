// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getWSTestURL(port) {
  return 'ws://localhost:' + port + '/echo-with-no-extension';
}

// Tries to: open a WebSocket, write a test message to it, close it. Verifies
// that all the necessary events are triggered if |expectedToConnect|, otherwise
// makes sure WebSocket terminates with an error.
function testWebSocketConnection(url, expectedToConnect) {
  var ws = new WebSocket(url);
  var kMessage = 'test message';

  var keepAlive = chrome.test.callbackAdded();

  ws.onerror = function(error) {
    chrome.test.log('WebSocket error: ' + error);
    chrome.test.assertFalse(expectedToConnect);
    keepAlive();
  };
  ws.onmessage = function(messageEvent) {
    chrome.test.log('Message received: ' + messageEvent.data);
    chrome.test.assertTrue(expectedToConnect);
    chrome.test.assertEq(kMessage, messageEvent.data);
    ws.close();
  };
  ws.onclose = function(event) {
    chrome.test.log('WebSocket closed.');
    chrome.test.assertEq(expectedToConnect, event.wasClean);
  }

  ws.onopen = function() {
    chrome.test.log('WebSocket opened.');
    chrome.test.assertTrue(expectedToConnect);
    keepAlive();
    ws.send(kMessage);
  };
}
