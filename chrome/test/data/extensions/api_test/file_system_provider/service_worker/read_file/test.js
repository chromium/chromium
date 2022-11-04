// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {catchError, mountTestFileSystem, openFile, readTextFromBlob, remoteProvider, startReadTextFromBlob} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;
  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Read contents of a file. This file exists, so it should succeed.
    async function readFileSuccess() {
      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_READ_SUCCESS,
          {create: false},
      );
      const file = await openFile(fileEntry);
      const text = await readTextFromBlob(file);

      chrome.test.assertEq(TestFileSystemProvider.INITIAL_TEXT, text);
      chrome.test.succeed();
    },

    // Read contents of a file multiple times at once. Verify that there is at
    // most as many opened files at once as permitted per limit.
    async function readFileWithOpenedFilesLimitSuccess() {
      await fileSystem.remount(/*openedFilesLimit=*/ 2);

      // Start N read requests in parallel.
      const N = 20;
      const reads = [];
      for (let i = 0; i < N; i++) {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_BLOCK_IO, {create: false});
        const file = await openFile(fileEntry);
        reads.push(readTextFromBlob(file));
      }
      // All reads will be stalled, unblock them one by one.
      // In theory there is a race: it is possible (although unlikely) for
      // requests to come through two at a time by chance, but it's not
      // possible to check without some sort of timeout (waiting for a read to
      // not happen in a period of time).
      for (let i = 0; i < N; i++) {
        const {requestId} =
            await remoteProvider.waitForEvent('onReadFileRequestedStalled');
        chrome.test.assertTrue(await remoteProvider.getOpenedFiles() <= 2);
        await remoteProvider.continueRequest(requestId);
      }

      // All reads should complete successfully.
      for (const promise of reads) {
        chrome.test.assertEq(
            TestFileSystemProvider.INITIAL_TEXT, await promise);
      }
      chrome.test.succeed();
    },

    // Read contents of a file, but with an error on the way. This should
    // result in an error.
    async function readFileError() {
      await fileSystem.remount(/*openedFilesLimit=*/ 0);

      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_FAIL,
          {create: false},
      );
      const file = await openFile(fileEntry);
      const error = await catchError(readTextFromBlob(file));

      chrome.test.assertTrue(
          !!error, 'Unexpectedly succeeded to read a broken file.');
      chrome.test.assertEq('NotReadableError', error.name);
      chrome.test.succeed();
    },

    // Abort reading a file with a registered abort handler. Should result in a
    // gracefully terminated reading operation.
    async function abortReadingSuccess() {
      await remoteProvider.resetState();

      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_BLOCK_IO,
          {create: false},
      );
      const file = await openFile(fileEntry);
      const {promise, reader} = startReadTextFromBlob(file);
      // Wait until the read request made it to the FSP.
      await remoteProvider.waitForEvent('onReadFileRequested');
      chrome.test.assertEq(1, await remoteProvider.getOpenedFiles());
      reader.abort();
      await remoteProvider.waitForEvent('onAbortRequested');
      await remoteProvider.waitForEvent('onCloseFileRequested');
      const error = await catchError(promise);

      chrome.test.assertEq(0, await remoteProvider.getOpenedFiles());
      chrome.test.assertTrue(
          !!error, 'Unexpectedly succeeded after read aborted.');
      chrome.test.assertEq('AbortError', error.name);
      chrome.test.succeed();
    },

    // Abort opening a file while trying to read it without an abort handler
    // wired up. This should cause closing the file anyway.
    async function abortViaCloseSuccess() {
      await remoteProvider.resetState();
      await remoteProvider.setHandlerEnabled('onAbortRequested', false);

      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_BLOCK_IO,
          {create: false, exclusive: false},
      );
      const file = await openFile(fileEntry);
      const {promise, reader} = startReadTextFromBlob(file);
      // Wait until the read request made it to the FSP.
      await remoteProvider.waitForEvent('onReadFileRequested');
      chrome.test.assertEq(1, await remoteProvider.getOpenedFiles());
      reader.abort();
      await remoteProvider.waitForEvent('onCloseFileRequested');
      const error = await catchError(promise);

      chrome.test.assertEq(0, await remoteProvider.getOpenedFiles());
      chrome.test.assertTrue(
          !!error, 'Unexpectedly succeeded after read aborted.');
      chrome.test.assertEq('AbortError', error.name);
      chrome.test.succeed();
    },

    // Abort opening a file while trying to read it without an abort handler
    // wired up, then quickly try to open it again while having a limit of 1
    // opened files at once. This is a regression test for: crbug.com/519063.
    async function abortOpenedAndReopenSuccess() {
      await remoteProvider.resetState();
      await fileSystem.remount(/*openedFilesLimit=*/ 1);
      await remoteProvider.setHandlerEnabled('onAbortRequested', false);

      // Start reading two files, the first read should get stuck on open.
      const fileEntry1 = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_BLOCK_OPEN,
          {create: false, exclusive: false},
      );
      const fileEntry2 = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_READ_SUCCESS,
          {create: false, exclusive: false},
      );
      const read1 = startReadTextFromBlob(await openFile(fileEntry1));
      const read2 = startReadTextFromBlob(await openFile(fileEntry2));
      const openRequest1 =
          await remoteProvider.waitForEvent('onOpenFileRequested');
      // Wait until the first open request is blocked.
      const {requestId} =
          await remoteProvider.waitForEvent('onOpenFileRequestedStalled');
      chrome.test.assertEq(1, await remoteProvider.getOpenedFiles());
      // Abort the read and let the open request go, which should proceed to
      // immediately close.
      read1.reader.abort();
      await remoteProvider.continueRequest(requestId);

      // The second read request should continue successfully.
      const openRequest2 =
          await remoteProvider.waitForEvent('onOpenFileRequested');
      chrome.test.assertEq(
          `/${TestFileSystemProvider.FILE_READ_SUCCESS}`,
          openRequest2.filePath);
      chrome.test.assertEq(
          openRequest2.requestId,
          (await remoteProvider.waitForEvent('onReadFileRequested'))
              .openRequestId);
      chrome.test.assertEq(
          TestFileSystemProvider.INITIAL_TEXT, await read2.promise)
      // The first file should have been closed.
      chrome.test.assertEq(
          openRequest1.requestId,
          (await remoteProvider.waitForEvent('onCloseFileRequested'))
              .openRequestId);
      chrome.test.assertEq(0, await remoteProvider.getOpenedFiles());
      chrome.test.succeed();
    },
  ]);
}

main();
