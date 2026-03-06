// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests maximized/fullscreen window setWindowBounds() result.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  async function logWindowBounds() {
    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    testRunner.log(`Window: ${bounds.left},${bounds.top} ${bounds.width}x${
        bounds.height} ${bounds.windowState}`);
  }

  testRunner.log(`Initial state:`);
  dp.Browser.setWindowBounds(
      {windowId, bounds: {left: 10, top: 10, width: 700, height: 500}});
  await logWindowBounds();

  for (const state of ['maximized', 'fullscreen']) {
    testRunner.log(`Setting '${state}' state`);
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});
    await logWindowBounds();

    testRunner.log(`Restoring to normal state:`);
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: `normal`}});
    await logWindowBounds();
  }

  testRunner.completeTest();
});
