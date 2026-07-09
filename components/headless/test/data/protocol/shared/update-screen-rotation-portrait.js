// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={600x800}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API portrait rotation handling.');

  async function getScreenId(index) {
    const {screenInfos} = (await dp.Emulation.getScreenInfos()).result;
    return screenInfos[index].id;
  }

  const screenId = await getScreenId(0);

  for (const rotation of [0, 90, 180, 270]) {
    const {screenInfo} = (await dp.Emulation.updateScreen({
                           screenId,
                           rotation,
                         })).result;
    testRunner.log(screenInfo, `Rotation=${rotation} degrees screen info: `);
  }

  testRunner.completeTest();
});
