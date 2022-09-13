// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This JS can run either in a worker or as a script in the main document
// context.
function connectWebSocketWithMessageCallback(url, messageCallback) {
  const ws = new WebSocket(url);

  ws.onopen = () => {
    ws.send('hello world');
  };

  ws.onclose = () => {
    messageCallback('onclose');
  };

  ws.onmessage = () => {
    messageCallback('onmessage');
  };
}
