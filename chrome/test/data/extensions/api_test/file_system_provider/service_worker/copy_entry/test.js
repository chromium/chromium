// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  const srcPath = TestFileSystemProvider.FILE_READ_SUCCESS;
  const dstPath = srcPath + '-copy';

  chrome.test.runTests([
    // Copy an existing file to a non-existing destination. Should succeed.
    async function copyEntrySuccess() {
      const sourceEntry =
          await fileSystem.getFileEntry(srcPath, {create: false});
      chrome.test.assertFalse(sourceEntry.isDirectory);

      const targetEntry = await new Promise(
          (resolve, reject) => sourceEntry.copyTo(
              fileSystem.fileSystem.root, dstPath, resolve, reject));

      chrome.test.assertEq(dstPath, targetEntry.name);
      chrome.test.assertFalse(targetEntry.isDirectory);
      chrome.test.succeed();
    },
    // Copy an existing file to a location which already holds a file.
    // Should fail.
    async function copyEntryExistsError() {
      const sourceEntry =
          await fileSystem.getFileEntry(srcPath, {create: false});
      chrome.test.assertFalse(sourceEntry.isDirectory);

      const error = await catchError(new Promise(
          (resolve, reject) => sourceEntry.copyTo(
              fileSystem.fileSystem.root, dstPath, resolve, reject)));

      chrome.test.assertTrue(!!error, 'Succeeded, but should fail.');
      chrome.test.assertEq('InvalidModificationError', error.name);
      chrome.test.succeed();
    },
  ]);
}

main();
