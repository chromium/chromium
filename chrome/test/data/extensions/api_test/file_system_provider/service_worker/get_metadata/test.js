// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/**
 * @param {!FileEntry|!DirectoryEntry} entry
 * @returns {!Promise<!Metadata>}
 */
async function getMetadata(entry) {
  return new Promise((resolve, reject) => entry.getMetadata(resolve, reject));
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Read metadata of the root.
    async function getRootMetadataSuccess() {
      try {
        const meta = await getMetadata(fileSystem.fileSystem.root);
        chrome.test.assertEq(0, meta.size);
        chrome.test.assertEq(
            new Date(2014, 4, 28, 10, 39, 15).toString(),
            meta.modificationTime.toString());
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Read metadata of an existing testing file.
    async function getFileMetadataSuccess() {
      try {
        await remoteProvider.resetState();
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_READ_SUCCESS,
            {create: false},
        );
        const meta = await getMetadata(fileEntry);
        chrome.test.assertEq(
            TestFileSystemProvider.INITIAL_TEXT.length, meta.size);
        chrome.test.assertEq(
            new Date(2014, 1, 25, 7, 36, 12).toString(),
            meta.modificationTime.toString());
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Read metadata of an existing testing file, which however has an invalid
    // modification time. It should not cause an error, but an invalid date
    // should be passed to fileapi instead. The reason is, that there is no
    // easy way to verify an incorrect modification time at early stage.
    async function getFileMetadataWrongTimeSuccess() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_INVALID_DATE,
            {create: false},
        );
        const meta = await getMetadata(fileEntry);
        chrome.test.assertTrue(Number.isNaN(meta.modificationTime.getTime()));
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Read metadata of a directory which does not exist, what should return an
    // error. DirectoryEntry.getDirectory() causes fetching metadata.
    async function getFileMetadataNotFound() {
      try {
        const TEST_DIR = 'non-existent';
        await remoteProvider.resetState();
        try {
          await fileSystem.getDirectoryEntry(
              TEST_DIR,
              {create: false},
          );
          chrome.test.fail('Getting a directory should have failed.');
        } catch (e) {
          chrome.test.assertEq('NotFoundError', e.name);
        }
        const {entryPath} =
            await remoteProvider.waitForEvent('onGetMetadataRequested');
        chrome.test.assertEq(`/${TEST_DIR}`, entryPath);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Read metadata of a file using getDirectory(). An error should be returned
    // because of type mismatching. DirectoryEntry.getDirectory() causes
    // fetching metadata.
    async function getFileMetadataWrongType() {
      try {
        await remoteProvider.resetState();
        try {
          await fileSystem.getDirectoryEntry(
              TestFileSystemProvider.FILE_READ_SUCCESS,
              {create: false},
          );
          chrome.test.fail('Getting a directory should have failed.');
        } catch (e) {
          chrome.test.assertEq('TypeMismatchError', e.name);
        }
        const {entryPath} =
            await remoteProvider.waitForEvent('onGetMetadataRequested');
        chrome.test.assertEq(
            `/${TestFileSystemProvider.FILE_READ_SUCCESS}`, entryPath);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Resolving a file should only request is_directory field.
    async function getMetadataForGetFile() {
      try {
        await remoteProvider.resetState();
        await fileSystem.getFileEntry(
            `/${TestFileSystemProvider.FILE_ONLY_TYPE}`,
            {create: false},
        );
        const options =
            await remoteProvider.waitForEvent('onGetMetadataRequested');
        chrome.test.assertEq(options.isDirectory, true);
        chrome.test.assertEq(options.name, false);
        chrome.test.assertEq(options.size, false);
        chrome.test.assertEq(options.modificationTime, false);
        chrome.test.assertEq(options.thumbnail, false);
        chrome.test.assertEq(options.mimeType, false);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Check that if a requested mandatory field is missing, then the error
    // callback is invoked.
    async function getMetadataMissingFields() {
      try {
        await remoteProvider.resetState();
        const fileEntry = await fileSystem.getFileEntry(
            `/${TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE}`,
            {create: false},
        );
        try {
          await getMetadata(fileEntry);
          chrome.test.fail('Getting metadata should have failed.');
        } catch (e) {
          chrome.test.assertEq('InvalidStateError', e.name);
        }
        const options =
            await remoteProvider.waitForEvent('onGetMetadataRequested');
        chrome.test.assertEq(options.isDirectory, true);
        chrome.test.assertEq(options.name, false);
        chrome.test.assertEq(options.size, false);
        chrome.test.assertEq(options.modificationTime, false);
        chrome.test.assertEq(options.thumbnail, false);
        chrome.test.assertEq(options.mimeType, false);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Fetch only requested fields.
    async function getEntryPropertiesFewFields() {
      try {
        await remoteProvider.resetState();
        const fileEntry = await fileSystem.getFileEntry(
            `/${TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE}`,
            {create: false},
        );
        const fileProperties = await new Promise(
            resolve => chrome.fileManagerPrivate.getEntryProperties(
                [fileEntry], ['size'], resolve));
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
      } catch (e) {
        chrome.test.fail(e);
      }
    },
  ]);
}

main();
