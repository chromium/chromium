// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600}

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests browser window size larger than the desktop.`);

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();

  await dp.Page.enable();

  const setBounds = {left: 0, top: 0, width: 4096, height: 2160};
  await dp.Browser.setWindowBounds({windowId, bounds: setBounds});
  testRunner.log(`set size: ${setBounds.width}x${setBounds.height}`);

  await dp.Page.onceFrameResized();

  const {result: {bounds}} = await dp.Browser.getWindowBounds({windowId});
  testRunner.log(`get size: ${bounds.width}x${bounds.height}`);

  const value =
      await session.evaluate(`window.outerWidth + 'x' + window.outerHeight`);
  testRunner.log(`window size: ${value}`);

  testRunner.completeTest();
});
