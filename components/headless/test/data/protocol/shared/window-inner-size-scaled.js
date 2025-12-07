// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={1600x1200 devicePixelRatio=2.0}

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Browser.setContentsSize() updating inner width and height.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  const resizePromise = session.evaluateAsync(`
    new Promise(resolve =>
        {window.addEventListener('resize', resolve, {once: true})})
  `);

  dp.Browser.setContentsSize({
    windowId,
    width: 700,
    height: 500,
  });

  await resizePromise;
  const innerSize = (await session.evaluate(`
    ({innerWidth, innerHeight})
  `));

  testRunner.log(innerSize, 'Inner window size: ');

  testRunner.completeTest();
});
