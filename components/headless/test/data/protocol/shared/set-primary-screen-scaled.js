// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label=1st devicePixelRatio=2} {400,300 label=2nd}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.setPrimaryScreen() API with scaling.');

  testRunner.log(
      (await dp.Emulation.getScreenInfos()).result,
      'Screens before primary screen is changed: ');

  const result = await dp.Emulation.setPrimaryScreen({screenId: '2'});
  testRunner.log(result, 'Emulation.setPrimaryScreen result: ');

  testRunner.log(
      (await dp.Emulation.getScreenInfos()).result,
      'Screens after primary screen is changed: ');

  testRunner.completeTest();
});
