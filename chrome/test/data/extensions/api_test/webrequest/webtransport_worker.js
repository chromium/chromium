// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage =
    async (message) => {
  switch (message.data.test) {
    case 'expectSessionEstablished':
      await expectSessionEstablished(message.data.url);
      return;
    case 'expectSessionFailed':
      await expectSessionFailed(message.data.url);
      return;
    default:
      postMessage(`Unknown test name: ${message.data.test}`);
  }
}

async function expectSessionEstablished(url) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
    postMessage('PASS');
  } catch (e) {
    postMessage(`Ready should not be rejected: ${e}`);
  }
}

async function expectSessionFailed(url) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
    postMessage('Ready should be rejected.');
  } catch (e) {
   if (e.name !== 'WebTransportError') {
      postMessage(`Error name should be WebTransportError but is ${e.name}.`);
    } else {
      postMessage('PASS');
    }
  }
}