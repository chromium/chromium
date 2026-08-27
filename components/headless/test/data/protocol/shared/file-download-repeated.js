// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests repeated file download.');

  const downloadPath = testRunner.params('downloadPath');

  function stabilizeFilePath(filePath) {
    if (!filePath) {
      return '';
    }
    const pathSeparator = filePath.includes('/') ? '/' : '\\';
    return `<some file path>/${filePath.split(pathSeparator).pop()}`;
  }

  testRunner.log('Setting download behavior to allow...');
  await dp.Browser.setDownloadBehavior({
    behavior: 'allow',
    downloadPath: downloadPath,
    eventsEnabled: true,
  });

  const kDownloadCount = 4;
  for (let i = 0; i < kDownloadCount; i++) {
    testRunner.log(`Downloading file #${1 + i} ...`);
    const [, completedEvent] = await Promise.all([
      dp.Browser.onceDownloadWillBegin(),
      dp.Browser.onceDownloadProgress(e => e.params.state === 'completed'),
      session.evaluate(`
        (() => {
          const a = document.createElement('a');
          a.href = 'data:text/plain;charset=utf-8,Hello_headless_world!';
          a.download = 'download.txt';
          document.body.appendChild(a);
          a.click();
        })()
      `),
    ]);

    // DevTools download manager delegate generates the target file path
    // directly from the download directory and suggested filename without
    // collision resolution, so repeated downloads overwrite the same file
    // without name deduplication.
    testRunner.log(
        `Downloaded: ${stabilizeFilePath(completedEvent.params.filePath)}`);
  }

  testRunner.completeTest();
});
