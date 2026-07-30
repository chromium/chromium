// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = async e => {
  const ws = new WebSocket(e.data.url);

  ws.onopen = () => postMessage('PASS');
  ws.onclose = () => postMessage('FAIL');
}
