// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(port => {
  // Store the port to prevent it from garbage collection, which may
  // result in closing the port.
  self.port = port;
  port.postMessage({ msg: 'Hello' });
});
