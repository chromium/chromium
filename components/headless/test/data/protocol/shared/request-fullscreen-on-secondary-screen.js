// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={label='1st screen'}{600x800 label='2nd screen'}
// META: --disable-popup-blocking

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests element request fullscreen on a secondary screen.');

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
          const win = window.open('/page2.html', '_blank',
              'left=820, top=20, width=400, height=200');
          if (!win) {
            console.log('Failed to open Page2');
          } else {
            win.addEventListener('load', async () => {
                const cs = (await win.getScreenDetails()).currentScreen;

                // Blink outerWidth|Height change asynchronously after 'resize'
                // event is fired and there seems to be no good way to avoid
                // race other then wait until they change sometime after the
                // 'resize' event is received.
                function tryLogWindowSize() {
                  if (win.outerWidth > 400) {
                    console.log('Page2 size: '
                        + win.outerWidth + 'x' + win.outerHeight
                        + ', screen: ' + cs.label
                        + ' ' + cs.width + 'x' + cs.height);
                  } else {
                    win.setTimeout(() => tryLogWindowSize(), 0);
                  }
                }

                win.addEventListener('resize', () => {
                  tryLogWindowSize();
                });

                const element = win.document.getElementById("fullscreen-div");
                element.requestFullscreen();
              });
          }
      </script>
    </html>
    `);

  httpInterceptor.addResponse('https://example.com/page2.html', `
        <body><div id="fullscreen-div">Page2 element</div></body>
      `);

  dp.Runtime.enable();
  const readyPromise = dp.Runtime.onceConsoleAPICalled();

  await dp.Browser.grantPermissions(
      {permissions: ['windowManagement', 'automaticFullscreen']});

  session.navigate('https://example.com/index.html');

  const message = (await readyPromise).params.args[0].value;
  testRunner.log(message);

  testRunner.completeTest();
});
