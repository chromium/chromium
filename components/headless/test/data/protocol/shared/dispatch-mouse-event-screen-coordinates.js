// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that screen coordinates are correctly applied for mouse events.');

  const windowLeft = 150;
  const windowTop = 200;
  const clientX = 40;
  const clientY = 60;

  const {result: {windowId}} = await dp.Browser.getWindowForTarget();
  await dp.Browser.setWindowBounds({
    windowId,
    bounds: {left: windowLeft, top: windowTop, width: 800, height: 600},
  });

  // Use a robust validation for screen coordinates. The Y-offset is
  // non-deterministic due to browser UI, so we use a relational check (>=).
  // The X-offset is predictable, allowing a strict check (===).
  const eventDataPromise = session.evaluateAsync(`
    new Promise(resolve => {
      document.addEventListener('mousedown', e => {
        resolve({
          clientX: e.clientX,
          clientY: e.clientY,
          isScreenXOffsetCorrect: (e.screenX - e.clientX) >= ${windowLeft},
          isScreenYOffsetCorrect: (e.screenY - e.clientY) >= ${windowTop}
        });
      }, { once: true });
    })
  `);

  // Force a round-trip to the renderer to ensure the event listener is
  // installed.
  await session.evaluate('void 0');

  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    x: clientX,
    y: clientY,
    button: 'left',
    clickCount: 1,
  });

  const eventData = await eventDataPromise;

  testRunner.log(eventData);
  testRunner.completeTest();
});
