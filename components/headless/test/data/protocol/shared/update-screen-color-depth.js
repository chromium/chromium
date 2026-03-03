// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API color depth handling.');

  const screenId = '1';

  const {screenInfo} =
      (await dp.Emulation.updateScreen({screenId, colorDepth: 32})).result;

  testRunner.log(screenInfo, 'Updated screen info: ');

  testRunner.completeTest();
});
