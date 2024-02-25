// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var disconnectCount = 0;

chrome.runtime.onConnectExternal.addListener((p) => {
  p.postMessage('connected');
  p.onMessage.addListener((m) => {
    if (m == 'disconnect') {
      p.disconnect();
    }
  });
  p.onDisconnect.addListener(() => {
    disconnectCount++;
  });
});
