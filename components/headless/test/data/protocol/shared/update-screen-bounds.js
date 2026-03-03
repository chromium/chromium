// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='1st'}{label='2nd'}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API bounds handling.');

  const screenId = '2';

  const {screenInfo} =
      (await dp.Emulation.updateScreen(
           {screenId, left: 800, top: 600, width: 600, height: 800}))
          .result;

  testRunner.log(screenInfo, 'Updated screen info: ');

  testRunner.completeTest();
});
