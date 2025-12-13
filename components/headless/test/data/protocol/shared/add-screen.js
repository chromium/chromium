// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 label=1st}

(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests CDP Emulation.addScreen() API.');

  // Add a screen at the right of the primary screen.
  await dp.Emulation.addScreen(
      {left: 800, top: 0, width: 600, height: 800, label: '2nd'});
  testRunner.log((await dp.Emulation.getScreenInfos()).result, 'Two screens: ');

  // Add an internal screen at the bottom of the primary screen.
  const result = (await dp.Emulation.addScreen({
                   left: 0,
                   top: 600,
                   width: 800,
                   height: 600,
                   label: '3nd',
                   isInternal: true,
                 })).result;
  testRunner.log(result, 'Added screen: ');

  testRunner.log(
      (await dp.Emulation.getScreenInfos()).result, 'Three screens: ');

  testRunner.completeTest();
});
