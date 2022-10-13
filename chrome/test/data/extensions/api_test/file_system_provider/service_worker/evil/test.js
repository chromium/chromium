// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {mountTestFileSystem, openFile, readTextFromBlob, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;
  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Tests that returning a too big chunk (4 times larger than the file size,
    // and also much more than requested 1 KB of data) makes the request fail.
    async function returnTooLargeChunk() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_TOO_LARGE_CHUNK,
            {create: false},
        );
        const file = await openFile(fileEntry);
        // Read 1 KB of data.
        const fileSlice = file.slice(0, 1024);
        try {
          await readTextFromBlob(fileSlice);
          chrome.test.fail('Reading should fail.');
        } catch (e) {
          chrome.test.assertEq('NotReadableError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Tests that calling a success callback with a non-existing request id
    // doesn't cause any harm.
    async function invalidCallback() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_INVALID_CALLBACK,
            {create: false},
        );
        const file = await openFile(fileEntry);
        // Read 1 KB of data.
        const fileSlice = file.slice(0, 1024);
        try {
          await readTextFromBlob(fileSlice);
          chrome.test.fail('Reading should fail.');
        } catch (e) {
          chrome.test.assertEq('NotReadableError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Test that reading from files with negative size is not allowed (empty
    // result is returned).
    async function negativeSize() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_NEGATIVE_SIZE,
            {create: false},
        );
        const file = await openFile(fileEntry);
        // Read 1 KB of data.
        const fileSlice = file.slice(0, 1024);
        const text = await readTextFromBlob(fileSlice);
        chrome.test.assertEq('', text);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Tests that accesses to  files containing ".." do not work.
    async function relativeName() {
      try {
        await fileSystem.getFileEntry(
            '../../../b.txt',
            {create: false},
        );
        chrome.test.fail('Opening a file should fail.');
      } catch (e) {
        chrome.test.assertEq('NotFoundError', e.name);
        // The call to open a file should not even reach the provider.
        chrome.test.assertEq(
            0, await remoteProvider.getEventCount('onFileOpenRequested'));
        chrome.test.succeed();
      }
    }
  ]);
}

main();
