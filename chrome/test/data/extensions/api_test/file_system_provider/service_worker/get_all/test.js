// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getAllFsInfos, getFsInfoById, mountTestFileSystem, openFile, remoteProvider, startReadTextFromBlob, unmount} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  chrome.test.runTests([
    // Verifies if getAll() returns the mounted file system.
    async function mountSuccess() {
      try {
        const fileSystem = await mountTestFileSystem(/*openedFileLimit*/ 2);
        // Start a read so we have at least one file open.
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_BLOCKS_FOREVER, {create: false});
        const file = await openFile(fileEntry);
        startReadTextFromBlob(file);
        await remoteProvider.waitForEvent('onOpenFileRequested');

        const fsInfos = await getAllFsInfos();
        chrome.test.assertEq(1, fsInfos.length);
        chrome.test.assertEq(
            TestFileSystemProvider.FILESYSTEM_ID, fsInfos[0].fileSystemId);
        chrome.test.assertEq('Test Filesystem', fsInfos[0].displayName);
        chrome.test.assertTrue(fsInfos[0].writable);
        chrome.test.assertEq(2, fsInfos[0].openedFilesLimit);
        chrome.test.assertEq(1, fsInfos[0].openedFiles.length);
        chrome.test.assertEq(
            `/${TestFileSystemProvider.FILE_BLOCKS_FOREVER}`,
            fsInfos[0].openedFiles[0].filePath);
        chrome.test.assertEq(
            chrome.fileSystemProvider.OpenFileMode.READ,
            fsInfos[0].openedFiles[0].mode);

        const fsInfo =
            await getFsInfoById(TestFileSystemProvider.FILESYSTEM_ID);
        chrome.test.assertEq(
            TestFileSystemProvider.FILESYSTEM_ID, fsInfo.fileSystemId);
        chrome.test.assertEq('Test Filesystem', fsInfo.displayName);
        chrome.test.assertTrue(fsInfo.writable);
        chrome.test.assertEq(2, fsInfo.openedFilesLimit);
        chrome.test.assertEq(1, fsInfo.openedFiles.length);
        chrome.test.assertEq(
            `/${TestFileSystemProvider.FILE_BLOCKS_FOREVER}`,
            fsInfo.openedFiles[0].filePath);
        chrome.test.assertEq(
            chrome.fileSystemProvider.OpenFileMode.READ,
            fsInfo.openedFiles[0].mode);

        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Verifies that after unmounting, the file system is not available in
    // getAll() list.
    async function unmountSuccess() {
      try {
        await unmount(TestFileSystemProvider.FILESYSTEM_ID);

        chrome.test.assertEq([], await getAllFsInfos());
        try {
          await getFsInfoById(TestFileSystemProvider.FILESYSTEM_ID);
          chrome.test.fail('Found the filesystem that was unmounted.');
        } catch (e) {
          chrome.test.assertEq('NOT_FOUND', e.message);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Verifies that if mounting fails, then the file system is not added to the
    // getAll() list.
    async function mountError() {
      try {
        try {
          await chrome.fileSystemProvider.mount(
              {fileSystemId: '', displayName: ''});
          chrome.test.fail('Mount operation should have failed.');
        } catch (e) {
          chrome.test.assertEq('INVALID_OPERATION', e.message);
        }
        chrome.test.assertEq([], await getAllFsInfos());
        try {
          await getFsInfoById(TestFileSystemProvider.FILESYSTEM_ID);
          chrome.test.fail('Found the filesystem that was unmounted.');
        } catch (e) {
          chrome.test.assertEq('NOT_FOUND', e.message);
        }
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },
  ]);
}

main();
