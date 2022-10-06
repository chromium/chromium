// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';


async function main() {
  await navigator.serviceWorker.ready;

  const {fileSystem} = await mountTestFileSystem();

  /**
   * @param {string} path
   * @param {{create: boolean, exclusive: boolean}} options
   * @returns {!Promise<!FileEntry>}
   */
  const getFileEntry = (path, options) => {
    return new Promise(
        (resolve, reject) =>
            fileSystem.root.getFile(path, options, resolve, reject));
  };

  const srcPath = TestFileSystemProvider.FILE_READ_SUCCESS;
  const dstPath = srcPath + '-copy';

  chrome.test.runTests([
    // Copy an existing file to a non-existing destination. Should succeed.
    async function copyEntrySuccess() {
      try {
        const sourceEntry = await getFileEntry(srcPath, {create: false});
        chrome.test.assertFalse(sourceEntry.isDirectory);
        const targetEntry = await new Promise(
            (resolve, reject) =>
                sourceEntry.copyTo(fileSystem.root, dstPath, resolve, reject));
        chrome.test.assertEq(dstPath, targetEntry.name);
        chrome.test.assertFalse(targetEntry.isDirectory);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },
    // Copy an existing file to a location which already holds a file.
    // Should fail.
    async function copyEntryExistsError() {
      try {
        const sourceEntry = await getFileEntry(srcPath, {create: false});
        chrome.test.assertFalse(sourceEntry.isDirectory);
        try {
          await new Promise(
              (resolve, reject) => sourceEntry.copyTo(
                  fileSystem.root, dstPath, resolve, reject));
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
