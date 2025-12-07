// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={600x800}

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests window screen size orientation.');

  const result = await session.evaluate('window.screen.orientation.type');
  testRunner.log('orientation=' + result);

  testRunner.completeTest();
});
