// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='#1'}{label='#2'}{0,600 label='#3'}{label='#4'}
//
(async function(testRunner) {
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

  async function moveWindowAndLogScreen(new_bounds) {
    await dp.Browser.setWindowBounds({windowId, bounds: new_bounds});
    await dp.Page.onceFrameResized();

    const {bounds} = (await dp.Browser.getWindowBounds({windowId})).result;
    const screen = await session.evaluateAsync(async () => {
      const cs = (await getScreenDetails()).currentScreen;
      return cs.label;
    });

    testRunner.log(
        `Window` +
        ` ${bounds.left},${bounds.top} ${bounds.width}x${bounds.height}` +
        `, screen ${screen}`);
  }

  await moveWindowAndLogScreen({left: 1, top: 1, width: 500, height: 300});
  await moveWindowAndLogScreen({left: 801, top: 1, width: 500, height: 301});
  await moveWindowAndLogScreen({left: 1, top: 601, width: 500, height: 302});
  await moveWindowAndLogScreen({left: 801, top: 601, width: 500, height: 303});

  testRunner.completeTest();
});
