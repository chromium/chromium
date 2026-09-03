// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cases that need no browser-side setup live here rather than in the C++
// apitest, which keeps only the ones needing two profiles or a non-policy
// install.
//
// These share one worker and there is no stopCapture yet, so the order
// matters: everything that must run without a session comes before the test
// that starts one.

const TOO_MANY_ORIGINS = new Array(129).fill('https://example.com');

chrome.test.runTests([
  async function unparsableOriginRejects() {
    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.startCapture({origins: ['invalid-origin']}),
        'Error: The origin filter contains an entry that is not a valid ' +
            'origin.');

    // A rejected call must not leave a session behind.
    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertFalse(result.active, 'rejected start left a session');

    chrome.test.succeed();
  },

  async function tooManyOriginsRejects() {
    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.startCapture({origins: TOO_MANY_ORIGINS}),
        'Error: The origin filter contains too many entries.');
    chrome.test.succeed();
  },

  async function startCaptureThenStatusIsActive() {
    await chrome.enterprise.webrtc.startCapture();

    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertEq(
        'boolean', typeof result.active, 'status has no active flag');
    chrome.test.assertTrue(result.active, 'status not active');

    chrome.test.succeed();
  },

  // Runs last: it relies on the session started above.
  async function secondStartRejectsAsAlreadyCapturing() {
    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.startCapture(),
        'Error: A capture session is already active for this extension.');

    // The existing session survives the rejected call.
    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertTrue(result.active, 'existing session was lost');

    chrome.test.succeed();
  },
]);
