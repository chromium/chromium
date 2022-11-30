// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getWSTestURL(port) {
  return 'ws://localhost:' + port + '/echo-with-no-extension';
}

// Creates a dedicated worker which makes a WebSocket request.
function testWebSocketConnection(url, expectedToConnect) {
  const keepAlive = chrome.test.callbackAdded();
  const worker = new Worker('websocket_worker.js');
  worker.postMessage({url, expectedToConnect});
  worker.onmessage = (message) => {
    worker.terminate();
    chrome.test.assertEq('PASS', message.data);
    keepAlive();
  };
}
