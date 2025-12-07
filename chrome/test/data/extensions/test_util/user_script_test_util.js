// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Waits for the userScripts API to be allowed before starting tests.
 *
 * Assumes the C++ side will intercept the 'ready' message and enable user
 * scripts for the extension before responding.
 */
export async function waitForUserScriptsAPIAllowed() {
  await chrome.test.sendMessage('ready');
  chrome.test.assertTrue(!!chrome.userScripts);
  chrome.test.succeed();
}
