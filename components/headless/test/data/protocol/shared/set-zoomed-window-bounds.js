// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}
//
// This test produces unexpected results in headless mode,
// see http://crbug.com/446247998.
// META: fork_headless_mode_expectations

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests maximized/fullscreen window setWindowBounds() result.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  async function logWindowBounds() {
    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    testRunner.log(`${bounds.left},${bounds.top} ${bounds.width}x${
        bounds.height} ${bounds.windowState}`);
  }

  for (const state of ['maximized', 'fullscreen']) {
    testRunner.log(`Setting '${state}' state`);
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});
    await logWindowBounds();

    const bounds =
        {left: 10, top: 10, width: 700, height: 500, windowState: `normal`};

    testRunner.log(bounds, `Setting bounds: `);
    dp.Browser.setWindowBounds({windowId, bounds});
    await logWindowBounds();
  }

  testRunner.completeTest();
});
