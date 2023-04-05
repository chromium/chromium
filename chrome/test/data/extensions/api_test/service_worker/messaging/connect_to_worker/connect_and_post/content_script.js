// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let port = chrome.runtime.connect();

// Post messages to keep the service worker alive.
port.postMessage({ msg: 'Hello' });
setInterval(() => {
  port.postMessage({ msg: 'Hello' });
}, 100); // Post message every 100ms to prolong SW lifetime.
