// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.currentVersion = 1;

// Obtain the version of the background context.
async function getCurrentVersionOfBackgroundContext() {
  const response = await chrome.runtime.sendMessage(
    'get-current-version');
  return response;
};
