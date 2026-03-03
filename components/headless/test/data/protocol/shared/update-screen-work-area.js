// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='1st'}{label='2nd'}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API work area handling.');

  const screenId = '2';

  const {screenInfo} =
      (await dp.Emulation.updateScreen({
        screenId,
        workAreaInsets: {top: 10, left: 20, bottom: 30, right: 40},
      })).result;

  testRunner.log(screenInfo, 'Updated screen info: ');

  testRunner.completeTest();
});
