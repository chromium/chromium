// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}{1200x1600}
// META: --window-size=800,600

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests window zoom on a secondary screen.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  dp.Browser.setWindowBounds({windowId, bounds: {left: 1600}});

  for (const state of ['maximized', 'fullscreen']) {
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});

    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    testRunner.log(`${bounds.left},${bounds.top} ${bounds.width}x${
        bounds.height} ${bounds.windowState}`);

    // Chrome does not like to switch between maximized and full screen window
    // states, so reset window back to normal state.
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  }

  testRunner.completeTest();
});
