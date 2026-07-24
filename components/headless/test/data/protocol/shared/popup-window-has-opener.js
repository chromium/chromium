// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --disable-popup-blocking

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} =
      await testRunner.startBlank('Tests pop up window has window.opener.');

  const result = await session.evaluate(`
        const win = window.open('about:blank', '_blank', 'popup');
        win && win.opener === window;
    `);

  testRunner.log(result ? 'PASS' : 'FAIL');

  testRunner.completeTest();
});
