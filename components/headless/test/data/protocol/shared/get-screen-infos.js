// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 label=1st}{600x800 label=2nd}

(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests CDP Emulation.getScreenInfos() API.');

  const {screenInfos} = (await dp.Emulation.getScreenInfos()).result;
  testRunner.log(screenInfos, 'screenInfos: ');

  testRunner.completeTest();
});
