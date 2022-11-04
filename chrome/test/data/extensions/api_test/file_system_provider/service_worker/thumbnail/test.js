// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/**
 * @type {Object}
 * @const
 */
const TESTING_WITH_VALID_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: 'valid-thumbnail.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: TestFileSystemProvider.VALID_THUMBNAIL,
});

/**
 * @type {Object}
 * @const
 */
const TESTING_ALWAYS_WITH_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: TestFileSystemProvider.FILE_ALWAYS_VALID_THUMBNAIL,
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: TestFileSystemProvider.VALID_THUMBNAIL,
});

/**
 * @type {Object}
 * @const
 */
const TESTING_WITH_INVALID_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-thumbnail.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: 'https://www.foobar.com/evil'
});

async function main() {
  await navigator.serviceWorker.ready;
  await remoteProvider.addFiles({
    [`/${TESTING_WITH_VALID_THUMBNAIL_FILE.name}`]: {
      metadata: TESTING_WITH_VALID_THUMBNAIL_FILE,
      contents: '',
    },
    [`/${TESTING_ALWAYS_WITH_THUMBNAIL_FILE.name}`]: {
      metadata: TESTING_ALWAYS_WITH_THUMBNAIL_FILE,
      contents: '',
    },
    [`/${TESTING_WITH_INVALID_THUMBNAIL_FILE.name}`]: {
      metadata: TESTING_WITH_INVALID_THUMBNAIL_FILE,
      contents: '',
    },
  });
  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Test if providers are notified that no thumbnail is requested when normal
    // metadata is requested.
    async function notRequestedAndNotProvidedThumbnailSuccess() {
      await fileSystem.getFileEntry(
          `/${TESTING_WITH_VALID_THUMBNAIL_FILE.name}`, {create: false});
      chrome.test.succeed();
    },

    // If providers return a thumbnail data despite not being requested for
    // that, then the operation must fail.
    async function notRequestedButProvidedThumbnailError() {
      const error = await catchError(fileSystem.getFileEntry(
          `/${TESTING_ALWAYS_WITH_THUMBNAIL_FILE.name}`, {create: false}));
      chrome.test.assertTrue(
          !!error,
          'Thumbnail returned when not requested should result in an ' +
              'error, but the operation succeeded.');
      chrome.test.assertEq('InvalidStateError', error.name);
      chrome.test.succeed();
    },

    // Thumbnails should be returned when available for private API request.
    async function getEntryPropertiesWithThumbnailSuccess() {
      const fileEntry = await fileSystem.getFileEntry(
          `/${TESTING_WITH_VALID_THUMBNAIL_FILE.name}`, {create: false});
      const fileProperties = await promisifyWithLastError(
          chrome.fileManagerPrivate.getEntryProperties, [fileEntry],
          ['thumbnailUrl', 'size', 'modificationTime']);
      chrome.test.assertEq(1, fileProperties.length);
      chrome.test.assertEq(
          TestFileSystemProvider.VALID_THUMBNAIL,
          fileProperties[0].thumbnailUrl);
      chrome.test.assertEq(4096, fileProperties[0].size);
      chrome.test.assertEq(
          new Date(2014, 4, 28, 10, 39, 15),
          new Date(fileProperties[0].modificationTime));
      chrome.test.succeed();
    },

    // Confirm that extensions are not able to pass an invalid thumbnail url,
    // including evil urls.
    async function getEntryPropertiesWithInvalidThumbnail() {
      const fileEntry = await fileSystem.getFileEntry(
          `/${TESTING_WITH_INVALID_THUMBNAIL_FILE.name}`, {create: false});
      const fileProperties = await promisifyWithLastError(
          chrome.fileManagerPrivate.getEntryProperties, [fileEntry],
          ['thumbnailUrl']);
      chrome.test.assertEq(1, fileProperties.length);
      // The results for an entry is an empty dictionary in
      // case of an error.
      chrome.test.assertEq(0, Object.keys(fileProperties[0]).length);
      chrome.test.succeed();
    },

    // Confirm that the thumbnail is not requested when not needed.
    async function getEntryPropertiesWithoutThumbnail() {
      const fileEntry = await fileSystem.getFileEntry(
          `/${TESTING_WITH_VALID_THUMBNAIL_FILE.name}`, {create: false});
      const fileProperties = await promisifyWithLastError(
          chrome.fileManagerPrivate.getEntryProperties, [fileEntry], ['size']);
      chrome.test.assertEq(1, fileProperties.length);
      chrome.test.assertFalse('thumbnailUrl' in fileProperties[0]);
      chrome.test.assertEq(4096, fileProperties[0].size);
      chrome.test.succeed();
    }
  ]);
}

main();
