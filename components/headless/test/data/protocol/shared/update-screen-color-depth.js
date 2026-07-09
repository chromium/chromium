// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests CDP Emulation.updateScreen() API color depth handling.');

  async function getScreenId(index) {
    const {screenInfos} = (await dp.Emulation.getScreenInfos()).result;
    return screenInfos[index].id;
  }

  const screenId = await getScreenId(0);

  const {screenInfo} =
      (await dp.Emulation.updateScreen({screenId, colorDepth: 32})).result;

  testRunner.log(screenInfo, 'Updated screen info: ');

  testRunner.completeTest();
});
