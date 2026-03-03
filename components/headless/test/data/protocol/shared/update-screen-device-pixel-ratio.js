// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={2000x1000 \
// META:   workAreaLeft=10 workAreaRight=20 workAreaTop=30 workAreaBottom=40}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API device pixel ratio handling.');

  const screenId = '1';

  const {screenInfo} =
      (await dp.Emulation.updateScreen({screenId, devicePixelRatio: 2})).result;

  testRunner.log(screenInfo, 'Updated screen info: ');

  testRunner.completeTest();
});
