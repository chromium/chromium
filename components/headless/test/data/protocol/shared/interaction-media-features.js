// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} =
      await testRunner.startBlank('Tests interaction media features.');

  // Note: '(any-pointer: coarse)' is intentionally omitted as it evaluates to
  // true on Windows hosts with a touch digitizer (e.g. win-arm64-rel).
  const queries = [
    '(pointer: none)',
    '(pointer: coarse)',
    '(pointer: fine)',
    '(any-pointer: none)',
    '(any-pointer: fine)',
    '(hover: none)',
    '(hover: hover)',
    '(any-hover: none)',
    '(any-hover: hover)',
  ];

  for (const query of queries) {
    const matches =
        await session.evaluate(`window.matchMedia('${query}').matches`);
    testRunner.log(`${query}: ${matches}`);
  }

  testRunner.completeTest();
});
