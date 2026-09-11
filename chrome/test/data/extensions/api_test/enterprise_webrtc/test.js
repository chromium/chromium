// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cases that need no browser-side setup live here rather than in the C++
// apitest, which keeps only the ones needing two profiles or a non-policy
// install.
//
// Each test owns its capture session: it starts what it needs and stops it
// before returning, so tests do not depend on run order.

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

  async function stopCaptureRejectsWithNoSession() {
    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.stopCapture(),
        'Error: No capture session is active for this extension.');
    chrome.test.succeed();
  },

  async function startCaptureThenStatusIsActive() {
    await chrome.enterprise.webrtc.startCapture();

    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertEq(
        'boolean', typeof result.active, 'status has no active flag');
    chrome.test.assertTrue(result.active, 'status not active');

    await chrome.enterprise.webrtc.stopCapture();

    chrome.test.succeed();
  },

  async function secondStartRejectsAsAlreadyCapturing() {
    await chrome.enterprise.webrtc.startCapture();

    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.startCapture(),
        'Error: A capture session is already active for this extension.');

    // The existing session survives the rejected call.
    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertTrue(result.active, 'existing session was lost');

    await chrome.enterprise.webrtc.stopCapture();

    chrome.test.succeed();
  },

  async function stopCaptureFiresEvent() {
    await chrome.enterprise.webrtc.startCapture();

    // stopCapture() resolving only means the function replied; the event is
    // delivered separately, so wait for it rather than assuming it arrived.
    const stopped =
        chrome.test.listenOnce(chrome.enterprise.webrtc.onCaptureStopped);

    await chrome.enterprise.webrtc.stopCapture();

    const result = await chrome.enterprise.webrtc.getCaptureStatus();
    chrome.test.assertFalse(result.active, 'status still active');

    await stopped;

    chrome.test.succeed();
  },

  async function secondStopRejectsAsNotCapturing() {
    await chrome.enterprise.webrtc.startCapture();
    await chrome.enterprise.webrtc.stopCapture();

    // Stopping an already-stopped session must fail the same way as
    // stopping with no session at all.
    await chrome.test.assertPromiseRejects(
        chrome.enterprise.webrtc.stopCapture(),
        'Error: No capture session is active for this extension.');
    chrome.test.succeed();
  },
]);
