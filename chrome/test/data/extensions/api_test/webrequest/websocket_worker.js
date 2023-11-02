// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = (message) => {
  const ws = new WebSocket(message.data.url);
  const expectedToConnect = message.data.expectedToConnect;
  const messageData = 'test message';

  ws.onerror = (error) => {
    postMessage(expectedToConnect ? 'FAIL; onerror' : 'PASS');
  };
  ws.onmessage = (message) => {
    if (!expectedToConnect || message.data !== messageData) {
      postMessage('FAIL; onmessage');
    }
    ws.close();
  };
  ws.onclose = (event) => {
    if (expectedToConnect) {
      postMessage(event.wasClean ? 'PASS' : 'FAIL; onclose');
    }
  };
  ws.onopen = () => {
    if (!expectedToConnect) {
      postMessage('FAIL; onopen');
    }
    ws.send(messageData);
  };
}
