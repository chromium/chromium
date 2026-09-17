// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('message', (event) => {
  if (event.data.command === 'fetch') {
    const replyPort = event.ports[0];
    event.waitUntil((async () => {
      try {
        await fetch(event.data.url, {mode: event.data.mode});
        replyPort.postMessage({status: 'SUCCESS'});
      } catch (e) {
        replyPort.postMessage({
          status: 'FAILURE',
          error: `${e.name}: ${e.message}`,
        });
      }
    })());
  }
});
