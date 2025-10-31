// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let transport;

self.onmessage = async e => {
  switch (e.data.method) {
    case 'fetch':
      try {
        const response = await fetch(e.data.url);
        if (!response.ok) {
          self.postMessage('bad response');
          return;
        }
        const text = await response.text();
        self.postMessage(text);
      } catch (error) {
        self.postMessage(`${error}`);
      }
      break;
    case 'webtransport-open':
      try {
        const port = e.data.port
        transport = new WebTransport(e.data.url);
        await transport.ready;
        postMessage('webtransport opened');
      } catch (error) {
        self.postMessage(`${error}`);
      }
      break;
    case 'webtransport-close':
      try {
        transport.close();
        await transport.closed;
        postMessage('webtransport closed');
      } catch (error) {
        self.postMessage(`${error}`);
      }
      break;
    default:
      self.postMessage('unknown method: ' + e.data.method);
      break;
  }
};
