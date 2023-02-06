// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Listens for a persistent port connection from another extension and informs
// the test when the port is connected.
let port;
chrome.runtime.onConnectExternal.addListener((portInternal) => {
  port = portInternal;
  chrome.test.sendMessage('Persistent port connected');
});
