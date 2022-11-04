// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getMetadata, mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Read metadata of the root.
    async function getRootMetadataSuccess() {
      const meta = await getMetadata(fileSystem.fileSystem.root);

      chrome.test.assertEq(0, meta.size);
      chrome.test.assertEq(
          new Date(2014, 4, 28, 10, 39, 15).toString(),
          meta.modificationTime.toString());
      chrome.test.succeed();
    },

    // Read metadata of an existing testing file.
    async function getFileMetadataSuccess() {
      await remoteProvider.resetState();

      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_READ_SUCCESS, {create: false});
      const meta = await getMetadata(fileEntry);

      chrome.test.assertEq(
          TestFileSystemProvider.INITIAL_TEXT.length, meta.size);
      chrome.test.assertEq(
          new Date(2014, 1, 25, 7, 36, 12).toString(),
          meta.modificationTime.toString());
      chrome.test.succeed();
    },

    // Read metadata of an existing testing file, which however has an invalid
    // modification time. It should not cause an error, but an invalid date
    // should be passed to fileapi instead. The reason is, that there is no
    // easy way to verify an incorrect modification time at early stage.
    async function getFileMetadataWrongTimeSuccess() {
      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_INVALID_DATE, {create: false});

      const meta = await getMetadata(fileEntry);

      chrome.test.assertTrue(Number.isNaN(meta.modificationTime.getTime()));
      chrome.test.succeed();
    },

    // Read metadata of a directory which does not exist, what should return an
    // error. DirectoryEntry.getDirectory() causes fetching metadata.
    async function getFileMetadataNotFound() {
      const TEST_DIR = 'non-existent';
      await remoteProvider.resetState();

      const error = await catchError(
          fileSystem.getDirectoryEntry(TEST_DIR, {create: false}));

      chrome.test.assertTrue(
          !!error, 'Getting a directory should have failed.');
      chrome.test.assertEq('NotFoundError', error.name);
      const {entryPath} =
          await remoteProvider.waitForEvent('onGetMetadataRequested');
      chrome.test.assertEq(`/${TEST_DIR}`, entryPath);
      chrome.test.succeed();
    },

    // Read metadata of a file using getDirectory(). An error should be returned
    // because of type mismatching. DirectoryEntry.getDirectory() causes
    // fetching metadata.
    async function getFileMetadataWrongType() {
      await remoteProvider.resetState();

      const error = await catchError(fileSystem.getDirectoryEntry(
          TestFileSystemProvider.FILE_READ_SUCCESS, {create: false}));

      chrome.test.assertTrue(
          !!error, 'Getting a directory should have failed.');
      chrome.test.assertEq('TypeMismatchError', error.name);
      const {entryPath} =
          await remoteProvider.waitForEvent('onGetMetadataRequested');
      chrome.test.assertEq(
          `/${TestFileSystemProvider.FILE_READ_SUCCESS}`, entryPath);
      chrome.test.succeed();
    },

    // Resolving a file should only request is_directory field.
    async function getMetadataForGetFile() {
      await remoteProvider.resetState();

      await fileSystem.getFileEntry(
          `/${TestFileSystemProvider.FILE_ONLY_TYPE}`, {create: false});

      const options =
          await remoteProvider.waitForEvent('onGetMetadataRequested');
      chrome.test.assertEq(options.isDirectory, true);
      chrome.test.assertEq(options.name, false);
      chrome.test.assertEq(options.size, false);
      chrome.test.assertEq(options.modificationTime, false);
      chrome.test.assertEq(options.thumbnail, false);
      chrome.test.assertEq(options.mimeType, false);
      chrome.test.succeed();
    },

    // Check that if a requested mandatory field is missing, then the error
    // callback is invoked.
    async function getMetadataMissingFields() {
      await remoteProvider.resetState();

      const fileEntry = await fileSystem.getFileEntry(
          `/${TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE}`,
          {create: false});

      const error = await catchError(getMetadata(fileEntry));

      chrome.test.assertTrue(!!error, 'Getting metadata should have failed.');
      chrome.test.assertEq('InvalidStateError', error.name);
      const options =
          await remoteProvider.waitForEvent('onGetMetadataRequested');
      chrome.test.assertEq(options.isDirectory, true);
      chrome.test.assertEq(options.name, false);
      chrome.test.assertEq(options.size, false);
      chrome.test.assertEq(options.modificationTime, false);
      chrome.test.assertEq(options.thumbnail, false);
      chrome.test.assertEq(options.mimeType, false);
      chrome.test.succeed();
    },

    // Fetch only requested fields.
    async function getEntryPropertiesFewFields() {
      await remoteProvider.resetState();

      const fileEntry = await fileSystem.getFileEntry(
          `/${TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE}`,
          {create: false});
      const fileProperties = await promisifyWithLastError(
          chrome.fileManagerPrivate.getEntryProperties, [fileEntry], ['size']);

      chrome.test.assertEq(1, fileProperties.length);
      chrome.test.assertEq(1024 * 4, fileProperties[0].size);
      // The first call is from getFileEntry.
      await remoteProvider.waitForEvent('onGetMetadataRequested');
      // The second call is from getEntryProperties.
      const options =
          await remoteProvider.waitForEvent('onGetMetadataRequested');
      chrome.test.assertEq(options.isDirectory, false);
      chrome.test.assertEq(options.name, false);
      chrome.test.assertEq(options.size, true);
      chrome.test.assertEq(options.modificationTime, false);
      chrome.test.assertEq(options.thumbnail, false);
      chrome.test.assertEq(options.mimeType, false);
      chrome.test.succeed();
    },
  ]);
}

main();
