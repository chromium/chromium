// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port;

chrome.runtime.onConnectExternal.addListener((p) => {
  // Save a "global" reference to the port so it can be used by the test later.
  port = p;
  p.postMessage('connected');
  p.onMessage.addListener((m) => {
    if (m == 'disconnect') {
      p.disconnect();
    }
  })
});
