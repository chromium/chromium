// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// META: --screen-info={800x600 label=1st}

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests CDP Emulation.addScreen() API with getScreenDetails().');

  const HttpInterceptor =
      await testRunner.loadScriptAbsolute('../resources/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse(
      'https://example.com/index.html', `<html></html>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  // Add a screen at the right of the primary screen.
  await dp.Emulation.addScreen(
      {left: 800, top: 0, width: 600, height: 800, label: '2nd'});

  const result = await session.evaluateAsync(async () => {
    const screenDetails = await getScreenDetails();
    const screenDetailed = screenDetails.screens.map(s => {
      const lines = [
        `Screen`,
        ` label='${s.label}'`,
        ` ${s.left},${s.top} ${s.width}x${s.height}`,
        ` avail=${s.availLeft},${s.availTop} ${s.availWidth}x${s.availHeight}`,
        ` isPrimary=${s.isPrimary}`,
        ` isExtended=${s.isExtended}`,
        ` isInternal=${s.isInternal}`,
        ` colorDepth=${s.colorDepth}`,
        ` devicePixelRatio=${s.devicePixelRatio}`,
        ` orientation.type=${s.orientation.type}`,
        ` orientation.angle=${s.orientation.angle}`,
      ];
      return lines.join('\n');
    });
    return screenDetailed.join('\n');
  });

  testRunner.log(result);

  testRunner.completeTest();
});
