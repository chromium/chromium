// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getAllFsInfos, mountTestFileSystem, promisifyWithLastError} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/** @param {!Entry} entry */
async function addFileWatch(entry) {
  return promisifyWithLastError(chrome.fileManagerPrivate.addFileWatch, entry);
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();
  const fileName = TestFileSystemProvider.FILE_READ_SUCCESS;

  chrome.test.runTests([
    // Add an entry watcher on an existing file.
    async function addWatcher() {
      const fileEntry =
          await fileSystem.getFileEntry(fileName, {create: false});
      const watching = await addFileWatch(fileEntry);

      chrome.test.assertTrue(watching);
      const fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(1, fsInfos[0].watchers.length);
      const watcher = fsInfos[0].watchers[0];
      chrome.test.assertEq(`/${fileName}`, watcher.entryPath);
      chrome.test.assertFalse(watcher.recursive);
      chrome.test.assertEq(undefined, /**@type {!Object} */ (watcher).tag);
      chrome.test.succeed();
    },

    // Add an entry watcher on a file which is already watched, it should fail.
    async function addExistingFileWatcher() {
      // Should start with one watcher.
      let fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(1, fsInfos[0].watchers.length);

      const fileEntry =
          await fileSystem.getFileEntry(fileName, {create: false});
      const error = await catchError(addFileWatch(fileEntry));

      chrome.test.assertTrue(
          !!error, 'Adding file watcher should have failed.');
      chrome.test.assertEq('Unknown error.', error.message);
      // Shouldn't change the number of watchers.
      fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(1, fsInfos[0].watchers.length);
      chrome.test.succeed();
    },

    // Add an entry watcher on a broken file, it should fail.
    async function addBrokenFileWatcher() {
      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_FAIL, {create: false});
      const error = await catchError(addFileWatch(fileEntry));

      chrome.test.assertTrue(
          !!error, 'Adding file watcher should have failed.');
      chrome.test.assertEq('Unknown error.', error.message);
      // Shouldn't change the number of watchers.
      const fsInfos = await getAllFsInfos();
      chrome.test.assertEq(1, fsInfos.length);
      chrome.test.assertEq(1, fsInfos[0].watchers.length);
      chrome.test.succeed();
    }
  ]);
}

main();
