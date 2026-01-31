// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener((message, sender) => {
  if (message === 'ping') {
    return Promise.resolve({response: 'pong'});
  }
});
