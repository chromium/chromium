// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='#1'}{label='#2'}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Target.createTarget() on a secondary screen.');

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (testRunner.createSessionFor(sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse(
      'https://example.com/index.html', '<html></html>');

  const {targetId} = (await session.protocol.Target.createTarget({
                       'url': 'about:blank',
                       'left': 800,
                       'top': 100,
                       'width': 500,
                       'height': 400,
                       'newWindow': true,
                     })).result;

  const createdTargetSession = await session.attachChild(targetId);

  await createdTargetSession.navigate('https://example.com/index.html');

  const screen =
      await createdTargetSession.evaluateAsync(async (expected_label) => {
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
      }, '#2');

  // Expect screen #2.
  testRunner.log(`Screen: ${screen}`);

  testRunner.completeTest();
});
