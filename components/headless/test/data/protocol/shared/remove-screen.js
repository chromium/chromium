// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 label=1st}{600x800 label=2nd}

(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests CDP Emulation.removeScreen() API.');

  // Invalid screen id.
  testRunner.log((await dp.Emulation.removeScreen({screenId: 'foobar'})).error);

  // Unknown screen id.
  testRunner.log(
      (await dp.Emulation.removeScreen({screenId: '123456789'})).error);

  const {screenInfos} = (await dp.Emulation.getScreenInfos()).result;
  const primaryScreenId = screenInfos[0].id;
  const secondaryScreenId = screenInfos[1].id;

  // Removing the primary screen is not allowed.
  testRunner.log(
      (await dp.Emulation.removeScreen({screenId: primaryScreenId})).error);

  // OK to remove the secondary screen.
  await dp.Emulation.removeScreen({screenId: secondaryScreenId});
  testRunner.log((await dp.Emulation.getScreenInfos()).result);

  // Removing the only screen in the system is not allowed.
  testRunner.log(
      (await dp.Emulation.removeScreen({screenId: primaryScreenId})).error);

  testRunner.completeTest();
});
