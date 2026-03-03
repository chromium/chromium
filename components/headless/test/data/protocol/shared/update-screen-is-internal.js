// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API isInternal handling.');

  const screenId = '1';

  for (const isInternal of [false, true, false, true]) {
    const {screenInfo} = (await dp.Emulation.updateScreen({
                           screenId,
                           isInternal,
                         })).result;
    testRunner.log(screenInfo, `isInternal=${isInternal} screen info: `);
  }

  testRunner.completeTest();
});
