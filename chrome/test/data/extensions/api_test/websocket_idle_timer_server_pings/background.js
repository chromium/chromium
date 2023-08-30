// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let socket;

// Opens a web socket, replying to the C++ caller when we receive the `open`
// event.
async function openSocket() {
  const port = (await chrome.test.getConfig()).testWebSocketPort;
  const url = `ws://localhost:${port}/send-message-every-quarter-second`;
  socket = new WebSocket(url);

  socket.onopen = () => {
    chrome.test.sendScriptResult('open');
  };
}

// Kicks off the connection to the web socket server, and then waits while the
// server messages back. Once complete, replies back to the C++ side.
async function perform2SecondsOfWebSocketActivity() {
  // IMPORTANT: We cannot use any APIs that extend service worker lifetime
  // (including APIs like chrome.test.sendMessage()) until after the two
  // seconds have passed. Otherwise, this would keep the service worker alive
  // and invalidate the test.
  const expectedMessage = 'ping';
  const start = performance.now();
  const waitForMs = 2 * 1000; // wait for 2 seconds.

  let reachedEnd = false;

  // Once the websocket closes, we message back to the C++. We expect this to
  // happen from this side closing the websocket; if the server closes the
  // connection, it should be considered an error (and the message is checked
  // on the C++ side).
  socket.onclose = () => {
    let endMessage = reachedEnd ? 'closed' : 'port unexpectedly closed';
    chrome.test.sendScriptResult(endMessage);
  };

  // Wait for 9 messages to come from the web socket. The port sends messages
  // every quarter second, so this should guarantee we've waited at least two
  // seconds (quite possibly more, depending on the speed of the bot).
  let receivedMessages = 0;
  socket.onmessage = ({data}) => {
    if (data != expectedMessage) {
      chrome.test.sendScriptResult(`unexpected message: ${data}`);
    }

    ++receivedMessages;

    // Close the port if we've received at least three messages *and* two
    // seconds have passed; otherwise, keep waiting.
    if (receivedMessages == 9) {
      const millis = performance.now() - start;
      if (millis > waitForMs) {
        reachedEnd = true;
      } else {
        console.error('Received messages from the server too quickly.');
      }

      socket.close();
    }
  };

  // Kick off the connection to have the web socket start pinging us. The
  // `9` indicates the port will send nine messages, each a quarter second
  // apart (i.e., a bit more than 2 seconds).
  socket.send(9);
}
