// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --disable-popup-blocking

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that opener is NOT specified on a page opened ' +
      'via Click with rel=noopener.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (testRunner.createSessionFor(sessionId)).protocol;

  const targetInfoResponse = await dp.Target.getTargetInfo();
  const initialTargetId =
      (targetInfoResponse.result || targetInfoResponse).targetInfo.targetId;

  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const targetAttachedPromise = new Promise(resolve => {
    bp.Target.onAttachedToTarget(event => {
      const targetInfo = event.params.targetInfo;
      if (targetInfo.type === 'page' &&
          targetInfo.targetId !== initialTargetId) {
        resolve(event.params);
      }
    });
  });

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse('https://example.com/index.html', `
      <html>
      <body style="margin: 0; padding: 0;">
        <a href="https://example.com/page2.html" target="_blank" rel="noopener"
           style="display: block; width: 100px; height: 100px;">Click</a>
      </body>
      </html>
  `);
  httpInterceptor.addResponse('https://example.com/page2.html', `
      <html><body>Page 2</body></html>
  `);

  await session.navigate('https://example.com/index.html');

  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    x: 50,
    y: 50,
    button: 'left',
    clickCount: 1,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    x: 50,
    y: 50,
    button: 'left',
    clickCount: 1,
  });

  const {sessionId: childSessionId} = await targetAttachedPromise;
  const childSession = testRunner.createSessionFor(childSessionId);
  const hasOpener = await childSession.evaluate('window.opener !== null');
  if (!hasOpener) {
    testRunner.log('PASS');
  } else {
    testRunner.log('FAIL: window.opener is not null');
  }

  testRunner.completeTest();
});
