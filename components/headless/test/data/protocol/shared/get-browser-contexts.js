// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank('Tests Target.getBrowserContexts');

  const browserSession = await testRunner.attachFullBrowserSession();
  const bp = browserSession.protocol;

  const {result: res1} = await bp.Target.getBrowserContexts();
  testRunner.log(
      `Initial browserContextIds count: ${res1.browserContextIds.length}`);
  testRunner.log(`Has defaultBrowserContextId: ${
      typeof res1.defaultBrowserContextId === 'string' &&
      res1.defaultBrowserContextId.length > 0}`);

  const {result: {browserContextId}} = await bp.Target.createBrowserContext();

  const {result: res2} = await bp.Target.getBrowserContexts();
  testRunner.log(
      `browserContextIds count after create: ${res2.browserContextIds.length}`);
  testRunner.log(`Contains created context: ${
      res2.browserContextIds.includes(browserContextId)}`);

  await bp.Target.disposeBrowserContext({browserContextId});

  const {result: res3} = await bp.Target.getBrowserContexts();
  testRunner.log(`browserContextIds count after dispose: ${
      res3.browserContextIds.length}`);

  testRunner.log('Attempting to dispose default browser context...');
  const disposeDefaultRes = await bp.Target.disposeBrowserContext({
    browserContextId: res1.defaultBrowserContextId,
  });
  const error = disposeDefaultRes.error;
  if (error && error.message) {
    error.message = error.message.replace(
        res1.defaultBrowserContextId, '<defaultBrowserContextId>');
  }
  testRunner.log(error, 'Error disposing default context:');

  testRunner.completeTest();
});
