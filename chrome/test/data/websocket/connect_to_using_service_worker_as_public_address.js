// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function handleMessage(e) {
  const ws = new WebSocket(e.data.url);
  ws.onopen = () => e.ports[0].postMessage('PASS');
  ws.onclose = () => e.ports[0].postMessage('FAIL');
}


self.addEventListener('message', e => {
  e.waitUntil(handleMessage(e));
});
