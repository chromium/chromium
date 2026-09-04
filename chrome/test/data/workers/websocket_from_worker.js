// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = e => {
  const ws = new WebSocket(e.data.url);
  ws.onmessage = msg => self.postMessage(msg.data);
  ws.onerror = () => self.postMessage('error');
};
