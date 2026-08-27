// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests file download.');

  const downloadPath = testRunner.params('downloadPath');

  function stabilizeFilePath(filePath) {
    if (!filePath) {
      return '';
    }
    const pathSeparator = filePath.includes('/') ? '/' : '\\';
    return `<some file path>/${filePath.split(pathSeparator).pop()}`;
  }

  function logObject(obj, title) {
    testRunner.log(obj, title, [...TestRunner.stabilizeNames, 'filePath']);
  }

  testRunner.log('Setting download behavior to allow...');
  await dp.Browser.setDownloadBehavior({
    behavior: 'allow',
    downloadPath: downloadPath,
    eventsEnabled: true,
  });

  testRunner.log('Downloading file by clicking an anchor link...');
  const [willBeginEvent, completedEvent] = await Promise.all([
    dp.Browser.onceDownloadWillBegin(),
    dp.Browser.onceDownloadProgress(e => e.params.state === 'completed'),
    session.evaluate(`
      const a = document.createElement('a');
      a.href = 'data:text/plain;charset=utf-8,Hello_headless_world!';
      a.download = 'download.txt';
      document.body.appendChild(a);
      a.click();
    `),
  ]);

  logObject(willBeginEvent, 'Browser.downloadWillBegin: ');
  logObject(completedEvent, 'Browser.downloadProgress: ');
  testRunner.log(`Downloaded file path: ${
      stabilizeFilePath(completedEvent.params.filePath)}`);

  testRunner.completeTest();
});
