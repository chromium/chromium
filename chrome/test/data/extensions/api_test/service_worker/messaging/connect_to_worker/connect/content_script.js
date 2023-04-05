// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Store the port to prevent it from garbage collection, which may result
// in closing the port.
window.port = chrome.runtime.connect();
window.port.onMessage.addListener(msg => {
  chrome.test.succeed();
});
