// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests window state transitions.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  await dp.Browser.setWindowBounds(
      {windowId, bounds: {left: 0, top: 0, width: 800, height: 600}});

  const windowStates = [
    'maximized',
    'normal',
    'fullscreen',
    'normal',
    'minimized',
    'normal',
    'maximized',
    'fullscreen',
    'minimized',
    'maximized',
    'minimized',
    'fullscreen',
    'minimized',
    'normal',
  ];

  async function getVisibilityState(expectedVisibility) {
    return await session.evaluateAsync(async (expected) => {
      if (document.visibilityState === expected) {
        return document.visibilityState;
      }
      return await new Promise(resolve => {
        const handler = () => {
          if (document.visibilityState === expected) {
            document.removeEventListener('visibilitychange', handler);
            resolve(document.visibilityState);
          }
        };
        document.addEventListener('visibilitychange', handler);
      });
    }, expectedVisibility);
  }

  for (const state of windowStates) {
    await dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});

    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    const expectedVisibility =
        (bounds.windowState === 'minimized') ? 'hidden' : 'visible';
    const visibilityState = await getVisibilityState(expectedVisibility);
    testRunner.log(`${bounds.left},${bounds.top} ${bounds.width}x${
        bounds.height} ${bounds.windowState} ${visibilityState}`);
  }

  testRunner.completeTest();
});
