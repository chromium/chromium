// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
self.addEventListener('connect', (event) => {
  const port = event.ports[0];
  port.addEventListener('message', (event) => {
    if (event.data === 'get-canvas-noise-token') {
      const token = CanvasInterventionsTest.getCanvasNoiseToken();
      port.postMessage(token);
    }
  });
  port.start();
});
