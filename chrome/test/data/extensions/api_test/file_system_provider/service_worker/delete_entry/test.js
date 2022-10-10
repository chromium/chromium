// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Delete a file. Should succeed.
    async function deleteDirectorySuccessSimple() {
      try {
        const entry =
            await fileSystem.getFileEntry(TestFileSystemProvider.FILE_DELETE, {
              create: false,
            });
        chrome.test.assertEq(TestFileSystemProvider.FILE_DELETE, entry.name);
        chrome.test.assertFalse(entry.isDirectory);
        await new Promise((resolve, reject) => entry.remove(resolve, reject));
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },
    // Delete a directory which has contents, non-recursively. Should fail.
    async function deleteDirectoryErrorNotEmpty() {
      try {
        const entry = await fileSystem.getDirectoryEntry(
            TestFileSystemProvider.TEST_DIR_DELETE_NONEMPTY, {create: false});
        chrome.test.assertEq(
            TestFileSystemProvider.TEST_DIR_DELETE_NONEMPTY, entry.name);
        chrome.test.assertTrue(entry.isDirectory);
        try {
          await new Promise((resolve, reject) => entry.remove(resolve, reject));
          chrome.test.fail('Unexpectedly succeded to remove a directory.');
        } catch (e) {
          chrome.test.assertEq('InvalidModificationError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Delete a directory which has contents, recursively. Should succeed.
    async function deleteDirectoryRecursively() {
      try {
        const entry = await fileSystem.getDirectoryEntry(
            TestFileSystemProvider.TEST_DIR_DELETE_NONEMPTY, {create: false});
        chrome.test.assertEq(
            TestFileSystemProvider.TEST_DIR_DELETE_NONEMPTY, entry.name);
        chrome.test.assertTrue(entry.isDirectory);
        await new Promise(
            (resolve, reject) => entry.removeRecursively(resolve, reject));
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },
  ]);
}

main();
