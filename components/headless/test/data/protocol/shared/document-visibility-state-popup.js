// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --disable-popup-blocking

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests popup window document.visibilityState.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <html>
      <script>
          const popup = window.open('/page2.html', '_blank',
              'popup, left=10, top=20, width=600, height=400');
          if (!popup) {
            console.log('Failed to create popup');
          } else {
            popup.addEventListener('load', async () => {
              console.log('Popup document.visibilityState: ' +
                  popup.document.visibilityState);
            });
          }
          </script>
      </html>
  `);

  httpInterceptor.addResponse(
      'https://example.com/page2.html', `<body>Page2</body>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  dp.Runtime.enable();

  for (let n = 0; n < 5; n++) {
    const readyPromise = dp.Runtime.onceConsoleAPICalled();
    await session.navigate('https://example.com/index.html');
    testRunner.log((await readyPromise).params.args[0].value);
    await session.navigate('about:blank');
  }

  testRunner.completeTest();
});
