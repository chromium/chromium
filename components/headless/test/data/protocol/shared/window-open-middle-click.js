// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --disable-popup-blocking

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests that middle click opens a new tab.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (testRunner.createSessionFor(sessionId)).protocol;

  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const targetAttachedPromise = bp.Target.onceAttachedToTarget();

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <html>
      <body style="margin: 0; padding: 0;">
        <a href="https://example.com/target.html"
           style="display: block; width: 100px; height: 100px;">Open target</a>
      </body>
      </html>
  `);
  httpInterceptor.addResponse('https://example.com/target.html', `
      <html><body><h1>Target page</h1></body></html>
  `);

  const targetRequestedPromise = bp.Fetch.onceRequestPaused(
      event => event.params.request.url.startsWith(
          'https://example.com/target.html'));

  await session.navigate('https://example.com/index.html');

  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    x: 50,
    y: 50,
    button: 'middle',
    clickCount: 1,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    x: 50,
    y: 50,
    button: 'middle',
    clickCount: 1,
  });

  // Wait for both the new target to attach and its navigation request to be
  // intercepted before completing the test to avoid race conditions during
  // test teardown while navigation IPCs are in-flight.
  await Promise.all([targetAttachedPromise, targetRequestedPromise]);

  testRunner.log('New tab opened');
  testRunner.completeTest();
});
