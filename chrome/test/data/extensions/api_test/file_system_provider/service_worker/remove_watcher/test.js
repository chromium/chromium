// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getAllFsInfos, mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/** @param {!Entry} entry */
async function addFileWatch(entry) {
  return promisifyWithLastError(chrome.fileManagerPrivate.addFileWatch, entry);
}

/** @param {!Entry} entry */
async function removeFileWatch(entry) {
  return promisifyWithLastError(
      chrome.fileManagerPrivate.removeFileWatch, entry);
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();
  const fileName = TestFileSystemProvider.FILE_READ_SUCCESS;
  const fileEntry = await fileSystem.getFileEntry(fileName, {create: false});

  chrome.test.runTests([
    // Add and remove an entry watcher on an existing file.
    async function removeWatcher() {
      // Add the file watch.
      const watching = await addFileWatch(fileEntry);
      chrome.test.assertTrue(watching);

      // Remove the file watch.
      const result = await removeFileWatch(fileEntry);
      chrome.test.assertTrue(result);

      const fsInfos = await getAllFsInfos();

      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(0, fsInfos[0].watchers.length);

      chrome.test.succeed();
    },

    // Remove a non-existing watcher, it should fail.
    async function removeNonExistingFileWatcher() {
      // Should start without watcher.
      let fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(0, fsInfos[0].watchers.length);

      const error = await catchError(removeFileWatch(fileEntry));
      chrome.test.assertTrue(!!error, 'Removing a watcher should have failed.');
      chrome.test.assertEq('Unknown error.', error.message);

      // Shouldn't change the number of watchers.
      fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(0, fsInfos[0].watchers.length);

      chrome.test.succeed();
    },

    // Add an entry watcher and tries removes it. The providing extension
    // returns an error, but the watcher should be removed anyway.
    async function removeBrokenFileWatcher() {
      // Add file watch.
      const watching = await addFileWatch(fileEntry);
      chrome.test.assertTrue(watching);

      // Configure the FSP to return error for remove watcher.
      await remoteProvider.setConfig(
          'onRemoveWatcherRequestedError',
          chrome.fileSystemProvider.ProviderError.INVALID_OPERATION,
      );

      // This API returns success
      const result = await removeFileWatch(fileEntry);
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(result);

      // Despite the "error" in the FSP, the watcher should be removed.
      const fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(0, fsInfos[0].watchers.length);

      chrome.test.succeed();
    }
  ]);
}

main();
