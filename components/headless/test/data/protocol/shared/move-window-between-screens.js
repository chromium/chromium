// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='#1'}{label='#2'}{0,600 label='#3'}{label='#4'}
//
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests window moving between multiple screens.');

  const {windowId} = (await dp.Browser.getWindowForTarget()).result;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse(
      'https://example.com/index.html', '<html></html>');

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  async function moveWindowAndLogScreen(new_bounds, expected_screen_label) {
    const screenPromise = session.evaluateAsync(async (expected_label) => {
      const sd = await getScreenDetails();
      // Check if the window is already on the expected screen, since
      // 'currentscreenchange' event will not fire if the screen did
      // not change.
      if (sd.currentScreen.label === expected_label) {
        return sd.currentScreen.label;
      }
      await new Promise(resolve => {
        const handler = () => {
          // Ignore any intermediate screen change events until the current
          // screen matches the expected one.
          if (sd.currentScreen.label === expected_label) {
            sd.removeEventListener('currentscreenchange', handler);
            resolve();
          }
        };
        sd.addEventListener('currentscreenchange', handler);
      });
      return sd.currentScreen.label;
    }, expected_screen_label);

    await dp.Browser.setWindowBounds({windowId, bounds: new_bounds});

    const screen = await screenPromise;
    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;

    testRunner.log(
        `Window` +
        ` ${bounds.left},${bounds.top} ${bounds.width}x${bounds.height}` +
        `, screen ${screen}`);
  }

  await moveWindowAndLogScreen(
      {left: 1, top: 1, width: 600, height: 300}, '#1');
  await moveWindowAndLogScreen(
      {left: 801, top: 1, width: 600, height: 300}, '#2');
  await moveWindowAndLogScreen(
      {left: 1, top: 601, width: 600, height: 300}, '#3');
  await moveWindowAndLogScreen(
      {left: 801, top: 601, width: 600, height: 300}, '#4');

  testRunner.completeTest();
});
