// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function echoTest(port) {
  var url = "ws://localhost:" + port + "/echo-with-no-extension";
  var ws = new WebSocket(url);
  var MESSAGE_A = "message a";
  var MESSAGE_B = "message b";

  ws.onopen = function() {
    chrome.test.log("websocket opened.");
    ws.send(MESSAGE_A);
  };

  ws.onclose = function() {
    chrome.test.log("websocket closed.");
  }

  ws.onmessage = function(messageEvent) {
    chrome.test.log("message received: " + messageEvent.data);
    chrome.test.assertEq(MESSAGE_A, messageEvent.data);

    ws.onmessage = function(messageEvent) {
      chrome.test.log("message received: " + messageEvent.data);
      chrome.test.assertEq(MESSAGE_B, messageEvent.data);
      ws.close();

      chrome.test.succeed();
    };

    ws.send(MESSAGE_B);
  };
}

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function runEchoTest() {
      echoTest(config.testWebSocketPort);
    }
  ]);
});

