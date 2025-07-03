// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session} =
      await testRunner.startBlank('Tests document.visibilityState upon start.');

  const result = await session.evaluate(`document.visibilityState`);

  testRunner.log('document.visibilityState: ' + result);

  testRunner.completeTest();
});
