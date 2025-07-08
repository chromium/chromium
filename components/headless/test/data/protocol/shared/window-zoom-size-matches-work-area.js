// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 \
// META:   workAreaLeft=10 workAreaRight=90 workAreaTop=20 workAreaBottom=80}

(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests maximized/fullscreen window matches workarea.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  for (const state of ['maximized', 'fullscreen']) {
    dp.Browser.setWindowBounds({windowId, bounds: {windowState: state}});

    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    testRunner.log(`${bounds.left},${bounds.top} ${bounds.width}x${
        bounds.height} ${bounds.windowState}`);
  }

  testRunner.completeTest();
});
