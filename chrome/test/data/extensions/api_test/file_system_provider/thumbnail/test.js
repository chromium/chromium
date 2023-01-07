// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_ROOT = Object.freeze({
  isDirectory: true,
  name: '',
  size: 0,
  modificationTime: new Date(2013, 3, 27, 9, 38, 14)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_WITH_VALID_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: 'valid-thumbnail.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA' +
             'AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO' +
             '9TXL0Y4OHwAAAABJRU5ErkJggg=='
});

/**
 * @type {Object}
 * @const
 */
var TESTING_ALWAYS_WITH_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: 'always-with-thumbnail.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA' +
             'AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO' +
             '9TXL0Y4OHwAAAABJRU5ErkJggg=='
});

/**
 * @type {Object}
 * @const
 */
var TESTING_WITH_INVALID_THUMBNAIL_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-thumbnail.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15),
  thumbnail: 'https://www.foobar.com/evil'
});

/**
 * Returns metadata for a requested entry.
 *
 * @param {GetMetadataRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  // Metadata to be returned.
  var metadata;

  switch (options.entryPath) {
    case '/':
      metadata = TESTING_ROOT;
      break;

    case '/' + TESTING_WITH_VALID_THUMBNAIL_FILE.name:
      metadata = TESTING_WITH_VALID_THUMBNAIL_FILE;
      break;

    case '/' + TESTING_ALWAYS_WITH_THUMBNAIL_FILE.name:
      metadata = TESTING_ALWAYS_WITH_THUMBNAIL_FILE;
      break;

    case '/' + TESTING_WITH_INVALID_THUMBNAIL_FILE.name:
      metadata = TESTING_WITH_INVALID_THUMBNAIL_FILE;
      break;

    default:
      onError('NOT_FOUND');  // enum ProviderError.
      return;
  }

  // Returning a thumbnail while not requested is not allowed for performance
  // reasons. Remove the field if needed. However, do not remove it for one
  // file, to simulate an error.
  if (!options.thumbnail && metadata.thumbnail &&
      options.entryPath !== '/' + TESTING_ALWAYS_WITH_THUMBNAIL_FILE.name) {
    var metadataWithoutThumbnail = {
      isDirectory: metadata.isDirectory,
      name: metadata.name,
      size: metadata.size,
      modificationTime: metadata.modificationTime
    };
    onSuccess(metadataWithoutThumbnail);
  } else {
    onSuccess(metadata);
  }
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      onGetMetadataRequested);
  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Test if providers are notified that no thumbnail is requested when normal
    // metadata is requested.
    function notRequestedAndNotProvidedThumbnailSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_WITH_VALID_THUMBNAIL_FILE.name,
          {create: false},
          chrome.test.callbackPass(),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // If providers return a thumbnail data despite not being requested for
    // that, then the operation must fail.
    function notRequestedButProvidedThumbnailError() {
      test_util.fileSystem.root.getFile(
          TESTING_ALWAYS_WITH_THUMBNAIL_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.fail(
                'Thumbnail returned when not requested should result in an ' +
                'error, but the operation succeeded.');
          }, chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('InvalidStateError', error.name);
          }));
    },

    // Thumbnails should be returned when available for private API request.
    function getEntryPropertiesWithThumbnailSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_WITH_VALID_THUMBNAIL_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.fileManagerPrivate.getEntryProperties(
                [fileEntry],
                ['thumbnailUrl', 'size', 'modificationTime'],
                chrome.test.callbackPass(function(fileProperties) {
                  chrome.test.assertEq(1, fileProperties.length);
                  chrome.test.assertEq(
                      TESTING_WITH_VALID_THUMBNAIL_FILE.thumbnail,
                      fileProperties[0].thumbnailUrl);
                  chrome.test.assertEq(
                      TESTING_WITH_VALID_THUMBNAIL_FILE.size,
                      fileProperties[0].size);
                  chrome.test.assertEq(
                      TESTING_WITH_VALID_THUMBNAIL_FILE.modificationTime,
                      new Date(fileProperties[0].modificationTime));
                }));
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
    },

    // Confirm that extensions are not able to pass an invalid thumbnail url,
    // including evil urls.
    function getEntryPropertiesWithInvalidThumbnail() {
      test_util.fileSystem.root.getFile(
          TESTING_WITH_INVALID_THUMBNAIL_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.fileManagerPrivate.getEntryProperties(
                [fileEntry],
                ['thumbnailUrl'],
                chrome.test.callbackPass(function(fileProperties) {
                  chrome.test.assertEq(1, fileProperties.length);
                  // The results for an entry is an empty dictionary in
                  // case of an error.
                  chrome.test.assertEq(
                      0, Object.keys(fileProperties[0]).length);
                }));
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
    },

    // Confirm that the thumbnail is not requested when not needed.
    function getEntryPropertiesWithoutThumbnail() {
      test_util.fileSystem.root.getFile(
          TESTING_WITH_VALID_THUMBNAIL_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.fileManagerPrivate.getEntryProperties(
                [fileEntry],
                ['size'],
                chrome.test.callbackPass(function(fileProperties) {
                  chrome.test.assertEq(1, fileProperties.length);
                  chrome.test.assertFalse(
                      'thumbnailUrl' in fileProperties[0]);
                  chrome.test.assertEq(
                      TESTING_WITH_VALID_THUMBNAIL_FILE.size,
                      fileProperties[0].size);
                }));
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
