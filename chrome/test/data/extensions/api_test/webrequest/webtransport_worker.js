// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  registerOnMessage(self);
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  // self is SharedWorkerGlobalScope.
  self.onconnect = (e) => {
    var port = e.ports[0];
    registerOnMessage(port);
    port.start();
  };
} else if (
    'ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  self.onmessage = (e) => {
    registerOnMessage(e.source);
    e.source.onmessage(e);
  };
}

function registerOnMessage(target) {
  target.onmessage = async (message) => {
    switch (message.data.test) {
      case 'expectSessionEstablished':
        await expectSessionEstablished(message.data.url, target);
        return;
      case 'expectSessionFailed':
        await expectSessionFailed(message.data.url, target);
        return;
      default:
        target.postMessage(`Unknown test name: ${message.data.test}`);
    }
  };
}

async function expectSessionEstablished(url, target) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
    target.postMessage('PASS');
  } catch (e) {
    target.postMessage(`Ready should not be rejected: ${e}`);
  }
}

async function expectSessionFailed(url, target) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
    target.postMessage('Ready should be rejected.');
  } catch (e) {
   if (e.name !== 'WebTransportError') {
     target.postMessage(
         `Error name should be WebTransportError but is ${e.name}.`);
    } else {
      target.postMessage('PASS');
    }
  }
}