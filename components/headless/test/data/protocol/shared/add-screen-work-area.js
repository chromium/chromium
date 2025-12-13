// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 label=1st}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.addScreen() API with work area.');

  // Add a screen at the right of the primary screen.
  await dp.Emulation.addScreen({
    left: 800,
    top: 0,
    width: 800,
    height: 600,
    label: '2nd',
    workAreaInsets: {top: 10, left: 11, bottom: 90, right: 89},
  });
  testRunner.log((await dp.Emulation.getScreenInfos()).result);

  testRunner.completeTest();
});
