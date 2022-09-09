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
var TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_WRONG_TIME_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-time.txt',
  size: 4096,
  modificationTime: new Date('Invalid date.')
});

/**
 * @type {Object}
 * @const
 */
var TESTING_ONLY_BASIC_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-time.txt',
});

/**
 * @type {string}
 * @const
 */
var TESTING_ONLY_BASIC_FILE_NAME = 'basic.txt';

/**
 * @type {Object}
 * @const
 */
var TESTING_ONLY_SIZE_FILE = Object.freeze({
  isDirectory: false,
  size: 4096
});

/**
 * @type {string}
 * @const
 */
var TESTING_ONLY_SIZE_FILE_NAME = 'only-size.txt';

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

  if (options.entryPath === '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (options.entryPath === '/' + TESTING_FILE.name) {
    onSuccess(TESTING_FILE);
    return;
  }

  if (options.entryPath === '/' + TESTING_WRONG_TIME_FILE.name) {
    onSuccess(TESTING_WRONG_TIME_FILE);
    return;
  }

  if (options.entryPath === '/' + TESTING_ONLY_BASIC_FILE_NAME) {
    onSuccess(TESTING_ONLY_BASIC_FILE);
    return;
  }

  if (options.entryPath === '/' + TESTING_ONLY_SIZE_FILE_NAME) {
    onSuccess(TESTING_ONLY_SIZE_FILE);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
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
    // Read metadata of the root.
    function getFileMetadataSuccess() {
      test_util.fileSystem.root.getMetadata(
        chrome.test.callbackPass(function(metadata) {
          chrome.test.assertEq(TESTING_ROOT.size, metadata.size);
          chrome.test.assertEq(
              TESTING_ROOT.modificationTime.toString(),
              metadata.modificationTime.toString());
        }), function(error) {
          chrome.test.fail(error.name);
        });
    },

    // Read metadata of an existing testing file.
    function getFileMetadataSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.test.assertEq(
                TESTING_FILE.isDirectory, fileEntry.isDirectory);
            fileEntry.getMetadata(chrome.test.callbackPass(function(metadata) {
              chrome.test.assertEq(TESTING_FILE.size, metadata.size);
              chrome.test.assertEq(
                  TESTING_FILE.modificationTime.toString(),
                  metadata.modificationTime.toString());
            }), function(error) {
              chrome.test.fail(error.name);
            });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Read metadata of an existing testing file, which however has an invalid
    // modification time. It should not cause an error, but an invalid date
    // should be passed to fileapi instead. The reason is, that there is no
    // easy way to verify an incorrect modification time at early stage.
    function getFileMetadataWrongTimeSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_WRONG_TIME_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_WRONG_TIME_FILE.name, fileEntry.name);
            fileEntry.getMetadata(chrome.test.callbackPass(function(metadata) {
              chrome.test.assertTrue(
                  Number.isNaN(metadata.modificationTime.getTime()));
            }), function(error) {
              chrome.test.fail(error.name);
            });
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Read metadata of a directory which does not exist, what should return an
    // error. DirectoryEntry.getDirectory() causes fetching metadata.
    function getFileMetadataNotFound() {
      test_util.fileSystem.root.getDirectory(
          'cranberries',
          {create: false},
          function(dirEntry) {
            chrome.test.fail();
          },
          chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('NotFoundError', error.name);
          }));
    },

    // Read metadata of a file using getDirectory(). An error should be returned
    // because of type mismatching. DirectoryEntry.getDirectory() causes
    // fetching metadata.
    function getFileMetadataWrongType() {
      test_util.fileSystem.root.getDirectory(
          TESTING_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.fail();
          },
          chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('TypeMismatchError', error.name);
          }));
    },

    // Resolving a file should only request is_directory and name fields.
    function getMetadataForGetFile() {
      test_util.fileSystem.root.getFile(
          TESTING_ONLY_BASIC_FILE_NAME,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertTrue(!!fileEntry);
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Check that if a requested mandatory field is missing, then the error
    // callback is invoked.
    function getMetadataMissingFields() {
      test_util.fileSystem.root.getFile(
          TESTING_ONLY_SIZE_FILE_NAME,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.getMetadata(
                function(metadata) {
                  chrome.test.fail('Unexpected success');
                },
                chrome.test.callbackPass(function(error) {
                  chrome.test.assertEq('InvalidStateError', error.name);
                }));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Fetch only requested fields.
    function getEntryPropertiesFewFields() {
      test_util.fileSystem.root.getFile(
          TESTING_ONLY_SIZE_FILE_NAME,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.fileManagerPrivate.getEntryProperties(
                [fileEntry],
                ['size'],
                chrome.test.callbackPass(function(fileProperties) {
                  chrome.test.assertEq(1, fileProperties.length);
                  chrome.test.assertEq(
                      TESTING_ONLY_SIZE_FILE.size, fileProperties[0].size);
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
