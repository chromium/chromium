// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests window ' +
      'state is properly adjusted upon Browser.setWindowSize.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  for (const state of ['minimized', 'maximized', 'fullscreen']) {
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});
    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    testRunner.log(
        `Window state: '${bounds.windowState}' (expected: '${state}')`);

    // Chrome does not allow transitions from non normal window states.
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: 'normal'}});
  }

  testRunner.completeTest();
});
