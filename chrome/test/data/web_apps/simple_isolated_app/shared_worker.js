// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Responds to post messages with the value of the *previous* message that was
// received.
let lastMessage = 'none';
self.addEventListener('connect', (e) => {
  const port = e.ports[0];

  port.addEventListener('message', (e) => {
    port.postMessage(lastMessage);
    lastMessage = e.data;
  });

  port.start();
});
