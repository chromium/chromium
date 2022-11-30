// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;
  const fileSystem = await mountTestFileSystem();

  const srcPath = TestFileSystemProvider.FILE_READ_SUCCESS;
  const dstPath = srcPath + '-moved';
  const FILE_MOVE_FAIL = 'move-fail.txt'

  await remoteProvider.addFiles({
    // Moving this file to a location which already holds a file will fail.
    [`/${FILE_MOVE_FAIL}`]: {
      metadata: {
        isDirectory: false,
        name: FILE_MOVE_FAIL,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
      },
    },
  });

  chrome.test.runTests([
    // Move an existing file to a non-existing destination. Should succeed.
    async function moveEntrySuccess() {
      try {
        const sourceEntry =
            await fileSystem.getFileEntry(srcPath, {create: false});
        chrome.test.assertFalse(sourceEntry.isDirectory);
        const targetEntry = await new Promise(
            (resolve, reject) => sourceEntry.moveTo(
                fileSystem.fileSystem.root, dstPath, resolve, reject));
        chrome.test.assertEq(dstPath, targetEntry.name);
        chrome.test.assertFalse(targetEntry.isDirectory);
        // The source file should be deleted.
        try {
          await fileSystem.getFileEntry(srcPath, {create: false});
          chrome.test.fail('Source file not deleted.');
        } catch (e) {
          chrome.test.assertEq('NotFoundError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Move an existing file to a location which already holds a file. Should
    // fail.
    async function moveEntryExistsError() {
      try {
        const sourceEntry =
            await fileSystem.getFileEntry(FILE_MOVE_FAIL, {create: false});
        chrome.test.assertFalse(sourceEntry.isDirectory);
        try {
          await new Promise(
              (resolve, reject) => sourceEntry.moveTo(
                  fileSystem.fileSystem.root, dstPath, resolve, reject));
          chrome.test.fail('Succeeded, but should fail.');
        } catch (e) {
          chrome.test.assertEq('InvalidModificationError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },
  ]);
}

main();