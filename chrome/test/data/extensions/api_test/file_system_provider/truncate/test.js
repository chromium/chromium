// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string}
 * @const
 */
var TESTING_TIRAMISU_FILE_NAME = 'tiramisu.txt';

/**
 * Requests truncating a file to the specified length.
 *
 * @param {TruncateRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onTruncateRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (!(options.filePath in test_util.defaultMetadata)) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  var metadata = test_util.defaultMetadata[options.filePath];

  // Truncating beyond the end of the file.
  if (options.length > metadata.size) {
    onError('INVALID_OPERATION');
    return;
  }

  metadata.size = options.length;
  onSuccess();
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
  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      test_util.onOpenFileRequested);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      test_util.onCloseFileRequested);
  chrome.fileSystemProvider.onCreateFileRequested.addListener(
      test_util.onCreateFileRequested);

  test_util.defaultMetadata['/' + TESTING_TIRAMISU_FILE_NAME] = {
    isDirectory: false,
    name: TESTING_TIRAMISU_FILE_NAME,
    size: 128,
    modificationTime: new Date(2014, 1, 24, 6, 35, 11)
  };

  chrome.fileSystemProvider.onTruncateRequested.addListener(
      onTruncateRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Truncate a file. It should succeed.
    function truncateFileSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE_NAME,
          {create: false, exclusive: true},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.createWriter(
                chrome.test.callbackPass(function(fileWriter) {
                  fileWriter.onwriteend = chrome.test.callbackPass(function(e) {
                    // Note that onwriteend() is called even if an error
                    // happened.
                    if (fileWriter.error)
                      return;
                    chrome.test.assertEq(
                        64,
                        test_util.defaultMetadata[
                            '/' + TESTING_TIRAMISU_FILE_NAME].size);
                  });
                  fileWriter.onerror = function(e) {
                    chrome.test.fail(fileWriter.error.name);
                  };
                  fileWriter.truncate(64);
                }),
                function(error) {
                  chrome.test.fail(error.name);
                });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Truncate a file to a length larger than size. This should result in an
    // error.
    function truncateBeyondFileError() {
      test_util.fileSystem.root.getFile(
          TESTING_TIRAMISU_FILE_NAME,
          {create: false, exclusive: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.createWriter(
                chrome.test.callbackPass(function(fileWriter) {
                  fileWriter.onwriteend = chrome.test.callbackPass(function(e) {
                    if (fileWriter.error)
                      return;
                    chrome.test.fail(
                        'Unexpectedly succeeded to truncate beyond a fiile.');
                  });
                  fileWriter.onerror = chrome.test.callbackPass(function(e) {
                    chrome.test.assertEq(
                        'InvalidModificationError', fileWriter.error.name);
                  });
                  fileWriter.truncate(test_util.defaultMetadata[
                      '/' + TESTING_TIRAMISU_FILE_NAME].size * 2);
                }),
                function(error) {
                  chrome.test.fail();
                });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
