// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onconnect = async function(event) {
  const port = event.ports[0];

  port.onmessage = async function(e) {
    const ws = new WebSocket(e.data.url);
    ws.onopen = () => port.postMessage('PASS');
    ws.onclose = () => port.postMessage('FAIL');
  };
};
