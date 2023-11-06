// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'kitty',
  size: 1024,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {string}
 * @const
 */
var TESTING_NEW_FILE_NAME = 'puppy.txt';

/**
 * Copies an entry within the same file system.
 *
 * @param {CopyEntryRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
function onCopyEntryRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.sourcePath === '/') {
    onError('INVALID_OPERATION');
    return;
  }

  if (!(options.sourcePath in test_util.defaultMetadata)) {
    onError('NOT_FOUND');
    return;
  }

  if (options.targetPath in test_util.defaultMetadata) {
    onError('EXISTS');
    return;
  }

  // Copy the metadata, but change the 'name' field.
  var newMetadata =
      structuredClone(test_util.defaultMetadata[options.sourcePath]);
  newMetadata.name = options.targetPath.split('/').pop();
  test_util.defaultMetadata[options.targetPath] = newMetadata;

  onSuccess();  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);

  test_util.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;

  chrome.fileSystemProvider.onCopyEntryRequested.addListener(
      onCopyEntryRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Copy an existing file to a non-existing destination. Should succeed.
    function copyEntrySuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: false},
          chrome.test.callbackPass(function(sourceEntry) {
            chrome.test.assertEq(TESTING_FILE.name, sourceEntry.name);
            chrome.test.assertFalse(sourceEntry.isDirectory);
            sourceEntry.copyTo(
                test_util.fileSystem.root,
                TESTING_NEW_FILE_NAME,
                chrome.test.callbackPass(function(targetEntry) {
                  chrome.test.assertEq(TESTING_NEW_FILE_NAME, targetEntry.name);
                  chrome.test.assertFalse(targetEntry.isDirectory);
                }), function(error) {
                  chrome.test.fail(error.name);
                });
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Copy an existing file to a location which already holds a file.
    // Should fail.
    function copyEntryExistsError() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: false},
          chrome.test.callbackPass(function(sourceEntry) {
            chrome.test.assertEq(TESTING_FILE.name, sourceEntry.name);
            chrome.test.assertFalse(sourceEntry.isDirectory);
            sourceEntry.copyTo(
                test_util.fileSystem.root,
                TESTING_NEW_FILE_NAME,
                function(targetEntry) {
                  chrome.test.fail('Succeeded, but should fail.');
                }, chrome.test.callbackPass(function(error) {
                  chrome.test.assertEq('InvalidModificationError', error.name);
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
