// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/169279800): Pull out test code that media app and help app have in
// common.

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
 * @type {!Promise<undefined>}
 */
const testMessageHandlersReady = new Promise(resolve => {
  window.addEventListener('DOMContentLoaded', () => {
    guestMessagePipe.registerHandler('test-handlers-ready', resolve);
  });
});

/**
 * Runs the given `testCase` in the guest context.
 * @param {string} testCase
 */
async function runTestInGuest(testCase) {
  /** @type {!TestMessageRunTestCase} */
  const message = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}
