// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, createWriter, getMetadata, mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * @param {!FileWriter} writer
 * @param {number} size
 * @returns {!Promise<void>}
 */
async function truncateFile(writer, size) {
  return new Promise((resolve, reject) => {
    writer.onwriteend = e => {
      // Note that onwriteend() is called even if an error
      // happened.
      if (!writer.error) {
        resolve();
      }
    };
    writer.onerror = e => reject(writer.error);
    writer.onabort = e => reject(writer.error);
    writer.truncate(size);
  });
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  const TEST_FILE = 'file-truncate.txt';
  const TEST_FILE_SIZE = 128;
  const TEST_FILE_SIZE_SMALLER = TEST_FILE_SIZE / 2;

  await remoteProvider.addFiles({
    [`/${TEST_FILE}`]: {
      metadata: {
        isDirectory: false,
        name: TEST_FILE,
        size: TEST_FILE_SIZE,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
      },
      contents: 'A'.repeat(TEST_FILE_SIZE),
    },
  });

  chrome.test.runTests([
    // Truncate a file. It should succeed.
    async function truncateFileSuccess() {
      const entry = await fileSystem.getFileEntry(
          TEST_FILE, {create: false, exclusive: true});
      chrome.test.assertEq(TEST_FILE, entry.name);
      const metadata = await getMetadata(entry);
      chrome.test.assertEq(TEST_FILE_SIZE, metadata.size);

      await truncateFile(await createWriter(entry), TEST_FILE_SIZE_SMALLER);

      const truncatedMetadata = await getMetadata(entry);
      chrome.test.assertEq(TEST_FILE_SIZE_SMALLER, truncatedMetadata.size);
      chrome.test.succeed();
    },

    // Truncate a file to a length larger than size. This should result in an
    // error.
    async function truncateBeyondFileError() {
      const entry = await fileSystem.getFileEntry(
          TEST_FILE, {create: false, exclusive: false});
      const metadata = await getMetadata(entry);
      chrome.test.assertEq(TEST_FILE_SIZE_SMALLER, metadata.size);

      const fileWriter = await createWriter(entry);
      const error = await catchError(truncateFile(fileWriter, TEST_FILE_SIZE));

      chrome.test.assertTrue(
          !!error, 'Unexpectedly succeeded to truncate beyond a file.');
      chrome.test.assertEq('InvalidModificationError', error.name);
      chrome.test.succeed();
    }
  ]);
}

main();