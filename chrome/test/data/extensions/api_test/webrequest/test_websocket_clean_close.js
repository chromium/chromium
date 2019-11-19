// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Opens a WebSocket, busy waits for 100ms, sends a message. Verifies that the
// close was clean. If it fails, it will fail flakily, so repeat it 10 times to
// get a deterministic answer.
function sendDoesntError(iteration = 0, done = undefined) {
  let ws = new WebSocket('ws://localhost:' + testWebSocketPort +
                         '/close-immediately');

  if (!done)
    done = chrome.test.callbackAdded();

  ws.onclose = event => {
    chrome.test.log('WebSocket ' + iteration + ' closed ' +
                    (event.wasClean ? 'cleanly.' : 'uncleanly.'));
    chrome.test.assertTrue(event.wasClean);
    if (iteration < 10) {
      ++iteration;
      sendDoesntError(iteration, done);
    } else {
      done();
    }
  }

  ws.onopen = () => {
    chrome.test.log('WebSocket ' + iteration + ' opened.');
    const start = performance.now();
    while (performance.now() - start < 100) {}
    ws.send('message');
  };
}

chrome.tabs.getCurrent(tab => runTestsForTab([sendDoesntError], tab));
