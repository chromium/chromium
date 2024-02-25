// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let socket;

// Opens a web socket, replying to the C++ caller when we receive the `open`
// event.
async function openSocket() {
  const port = (await chrome.test.getConfig()).testWebSocketPort;
  const url = `ws://localhost:${port}/echo-with-no-extension`;
  socket = new WebSocket(url);

  socket.onopen = () => {
    chrome.test.sendScriptResult('open');
  };
}

// Sends messages to the (previously-opened) web socket for two seconds, and
// then replies to the C++ caller.
async function perform2SecondsOfWebSocketActivity() {
  // IMPORTANT: We cannot use any APIs that extend service worker lifetime
  // (including APIs like chrome.test.sendMessage()) until after the two
  // seconds have passed. Otherwise, this would keep the service worker alive
  // and invalidate the test.
  const MESSAGE = 'test message';
  const start = performance.now();
  const waitForMs = 2 * 1000; // wait for 2 seconds.

  let reachedEnd = false;

  socket.onclose = () => {
    const endMessage = reachedEnd ? 'closed' : 'port unexpectedly closed';
    chrome.test.sendScriptResult(endMessage);
  };

  // Send messages back and forth to the web socket for two seconds.
  socket.onmessage = (messageEvent) => {
    if (messageEvent.data != MESSAGE) {
      chrome.test.sendScriptResult(`unexpected message: ${messageEvent.data}`);
    }

    // Close the port if two seconds have passed; otherwise, keep messaging.
    const millis = performance.now() - start;
    if (millis > waitForMs) {
      reachedEnd = true;
      socket.close();
    } else {
      socket.send(MESSAGE);
    }
  };

  socket.send(MESSAGE);
}
