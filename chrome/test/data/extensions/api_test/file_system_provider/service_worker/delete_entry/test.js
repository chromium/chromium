// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  const TEST_DIR = 'non-empty-directory';
  const TEST_FILE = 'file-delete.txt';

  await remoteProvider.addFiles({
    // Non-empty directory (can only be deleted recursively).
    [`/${TEST_DIR}`]: {
      metadata: {
        isDirectory: true,
        name: TEST_DIR,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
      },
    },
    [`/${TEST_DIR}/readme.txt`]: {
      metadata: {
        isDirectory: false,
        name: TEST_FILE,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
        size: 0,
      },
      contents: '',
    },
    [`/${TEST_FILE}`]: {
      metadata: {
        isDirectory: false,
        name: TEST_FILE,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
        size: 0,
      },
      contents: '',
    },
  });

  chrome.test.runTests([
    // Delete a file. Should succeed.
    async function deleteFileSuccessSimple() {
      const entry = await fileSystem.getFileEntry(TEST_FILE, {
        create: false,
      });
      chrome.test.assertEq(TEST_FILE, entry.name);
      chrome.test.assertFalse(entry.isDirectory);

      await new Promise((resolve, reject) => entry.remove(resolve, reject));

      chrome.test.succeed();
    },
    // Delete a directory which has contents, non-recursively. Should fail.
    async function deleteDirectoryErrorNotEmpty() {
      const entry =
          await fileSystem.getDirectoryEntry(TEST_DIR, {create: false});
      chrome.test.assertEq(TEST_DIR, entry.name);
      chrome.test.assertTrue(entry.isDirectory);

      const error = await catchError(
          new Promise((resolve, reject) => entry.remove(resolve, reject)));

      chrome.test.assertTrue(
          !!error, 'Unexpectedly succeded to remove a directory.');
      chrome.test.assertEq('InvalidModificationError', error.name);
      chrome.test.succeed();
    },

    // Delete a directory which has contents, recursively. Should succeed.
    async function deleteDirectoryRecursively() {
      const entry =
          await fileSystem.getDirectoryEntry(TEST_DIR, {create: false});
      chrome.test.assertEq(TEST_DIR, entry.name);
      chrome.test.assertTrue(entry.isDirectory);

      await new Promise(
          (resolve, reject) => entry.removeRecursively(resolve, reject));

      chrome.test.succeed();
    },
  ]);
}

main();
