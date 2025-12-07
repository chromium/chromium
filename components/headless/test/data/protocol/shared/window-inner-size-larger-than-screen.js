// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests Browser.setContentsSize larger than the current screen.`);

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await dp.Page.enable();

  const width = 3840;
  const height = 2160;
  await dp.Browser.setContentsSize({windowId, width, height});
  testRunner.log(`set contents size: ${width}x${height}`);

  await dp.Page.onceFrameResized();

  const value =
      await session.evaluate(`window.innerWidth + 'x' + window.innerHeight`);
  testRunner.log(`get contents size: ${value}`);

  testRunner.completeTest();
});
