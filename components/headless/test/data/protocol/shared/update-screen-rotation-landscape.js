// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API landscape rotation handling.');

  const screenId = '1';

  for (const rotation of [0, 90, 180, 270]) {
    const {screenInfo} = (await dp.Emulation.updateScreen({
                           screenId,
                           rotation,
                         })).result;
    testRunner.log(screenInfo, `Rotation=${rotation} degrees screen info: `);
  }

  testRunner.completeTest();
});
