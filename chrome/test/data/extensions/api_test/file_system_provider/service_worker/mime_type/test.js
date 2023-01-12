// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * @param {!FileEntry} fileEntry
 * @returns {!Promise<!chrome.fileManagerPrivate.ResultingTasks>}
 */
async function getFileTasks(fileEntry) {
  return promisifyWithLastError(
      resolve =>
          chrome.fileManagerPrivate.getFileTasks([fileEntry], [''], resolve));
}

/**
 * @param {!FileEntry} fileEntry
 * @returns {!Promise<void>}
 */
async function executeTask(fileEntry, taskDescriptor) {
  return promisifyWithLastError(
      chrome.fileManagerPrivate.executeTask, taskDescriptor, [fileEntry]);
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  const testFileName = 'mime.txt';
  const testMimeType = 'text/secret-testing-mime-type';
  const testFileNoMimeType = 'no-mime.txt';

  await remoteProvider.addFiles({
    [`/${testFileName}`]: {
      metadata: {
        isDirectory: false,
        name: testFileName,
        size: 0,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
        mimeType: testMimeType,
      },
      contents: '',
    },
    [`/${testFileNoMimeType}`]: {
      metadata: {
        isDirectory: false,
        name: testFileNoMimeType,
        size: 0,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
      },
      contents: '',
    },
  })

  chrome.test.runTests([
    // Test if the file with a MIME type handled by another app appears on a
    // task list.
    async function withMimeIsTask() {
      let fileEntry =
          await fileSystem.getFileEntry(`/${testFileName}`, {create: false});

      const result = await getFileTasks(fileEntry);

      chrome.test.assertEq(1, result.tasks.length);
      const task = result.tasks[0];
      chrome.test.assertEq(
          'gfnblenhaahcnmfdbebgincjohfkbnch', task.descriptor.appId);
      chrome.test.assertEq('app', task.descriptor.taskType);
      chrome.test.assertEq('magic_handler', task.descriptor.actionId);
      chrome.test.succeed();
    },

    // Confirm, that executing that task, will actually launch the test app
    // registered to handle this MME type. This is another code path, than
    // collecting tasks (tested above).
    async function withMimeExecute() {
      let fileEntry =
          await fileSystem.getFileEntry(`/${testFileName}`, {create: false});
      const nextMessage = new Promise(resolve => {
        const listener = (msg, sender, sendResponse) => {
          resolve(msg);
          chrome.runtime.onMessageExternal.removeListener(listener);
        };
        chrome.runtime.onMessageExternal.addListener(listener);
      });

      const result = await getFileTasks(fileEntry);
      await executeTask(fileEntry, result.tasks[0].descriptor);
      const launchEvent = await nextMessage;

      chrome.test.assertEq('magic_handler', launchEvent.id);
      chrome.test.assertEq(1, launchEvent.items.length);
      chrome.test.assertEq(testMimeType, launchEvent.items[0].type);
      chrome.test.assertEq(testFileName, launchEvent.items[0].entryName);
      chrome.test.succeed();
    },

    // The test app must not appear on the task list for the file without a MIME
    // type set.
    async function withoutMime() {
      let fileEntry = await fileSystem.getFileEntry(
          `/${testFileNoMimeType}`, {create: false});
      const result = await getFileTasks(fileEntry);

      chrome.test.assertEq(0, result.tasks.length);
      chrome.test.succeed();
    },
  ]);
}

main();
