// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This script runs within Dedicated, Shared, and Service Workers
 * to perform various network requests (fetch, WebSocket, WebTransport) for
 * interception testing.
 */

const kTestFunctions = {
  'ping': async (data) => {
      // Do nothing.
  },
  'fetch': async (data) => {
    await fetch(data.url);
  },
  'WebSocket': async (data) => {
    const ws = new WebSocket(data.url);
    await new Promise((resolve) => {
      ws.addEventListener('open', resolve);
    });
    ws.close();
  },
  'WebTransport': async (data) => {
    const transport = new WebTransport(data.url);
    await transport.ready;
    transport.close();
  }
};

async function handleMessage(data, replyPort) {
  try {
    await kTestFunctions[data.test](data);
    replyPort.postMessage('OK');
  } catch (e) {
    replyPort.postMessage('Failed');
  }
}

// For Dedicated Worker and Service Worker.
self.addEventListener('message', async (event) => {
  const replyPort = event.ports[0] || self;
  handleMessage(event.data, replyPort);
});

// For Shared Worker.
self.addEventListener('connect', (event) => {
  const port = event.ports[0];
  port.addEventListener('message', (msgEvent) => {
    handleMessage(msgEvent.data, port);
  });
  port.start();
});
