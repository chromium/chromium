// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object<number, string>}
 */
var openedFiles = {};

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_TOO_LARGE_CHUNK_FILE = Object.freeze({
  isDirectory: false,
  name: 'too-large-chunk.txt',
  size: 2 * 1024 * 1024,  // 2MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_INVALID_CALLBACK_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-request.txt',
  size: 1 * 1024 * 1024,  // 1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_NEGATIVE_SIZE_FILE = Object.freeze({
  isDirectory: false,
  name: 'negative-size.txt',
  size: -1 * 1024 * 1024,  // -1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_RELATIVE_NAME_FILE = Object.freeze({
  isDirectory: false,
  name: '../../../b.txt',
  size: 1 * 1024 * 1024,  // 1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Requests opening a file at <code>filePath</code>. Further file operations
 * will be associated with the <code>requestId</code>
 *
 * @param {OpenFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onOpenFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (options.mode !== 'READ') {
    onError('ACCESS_DENIED');  // enum ProviderError.
    return;
  }

  if (options.filePath !== '/' + TESTING_TOO_LARGE_CHUNK_FILE.name &&
      options.filePath !== '/' + TESTING_INVALID_CALLBACK_FILE.name &&
      options.filePath !== '/' + TESTING_NEGATIVE_SIZE_FILE.name &&
      options.filePath !== '/' + TESTING_RELATIVE_NAME_FILE.name) {
    onError('NOT_FOUND');  // enum ProviderError.
    return;
  }

  openedFiles[options.requestId] = options.filePath;
  onSuccess();
}

/**
 * Requests closing a file previously opened with <code>openRequestId</code>.
 *
 * @param {CloseFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onCloseFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID ||
      !openedFiles[options.openRequestId]) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  delete openedFiles[options.openRequestId];
  onSuccess();
}

/**
 * Requests reading contents of a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {ReadFileRequestedOptions} options Options.
 * @param {function(ArrayBuffer, boolean)} onSuccess Success callback with a
 *     chunk of data, and information if more data will be provided later.
 * @param {function(string)} onError Error callback.
 */
function onReadFileRequested(options, onSuccess, onError) {
  var filePath = openedFiles[options.openRequestId];
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID || !filePath) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (filePath === '/' + TESTING_TOO_LARGE_CHUNK_FILE.name) {
    var buffer = '';
    while (buffer.length < 4 * TESTING_TOO_LARGE_CHUNK_FILE.size) {
      buffer += 'I-LIKE-ICE-CREAM!';
    }
    var reader = new FileReader();
    reader.onload = function(e) {
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, false /* hasMore */);
    };
    reader.readAsArrayBuffer(new Blob([buffer]));
    return;
  }

  if (filePath === '/' + TESTING_INVALID_CALLBACK_FILE.name) {
    // Calling onSuccess after onError is unexpected. After handling the error
    // the request should be removed.
    onError('NOT_FOUND');
    onSuccess(new ArrayBuffer(options.length * 4), false /* hasMore */);
    return;
  }

  if (filePath === '/' + TESTING_NEGATIVE_SIZE_FILE.name) {
    onSuccess(new ArrayBuffer(-TESTING_NEGATIVE_SIZE_FILE.size * 2),
              false /* hasMore */);
    return;
  }

  if (filePath === '/' + TESTING_RELATIVE_NAME_FILE.name) {
    onSuccess(new ArrayBuffer(options.length), false /* hasMore */);
    return;
  }

  onError('INVALID_OPERATION');  // enum ProviderError.
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

  test_util.defaultMetadata['/' + TESTING_TOO_LARGE_CHUNK_FILE.name] =
    TESTING_TOO_LARGE_CHUNK_FILE;
  test_util.defaultMetadata['/' + TESTING_INVALID_CALLBACK_FILE.name] =
    TESTING_INVALID_CALLBACK_FILE;
  test_util.defaultMetadata['/' + TESTING_NEGATIVE_SIZE_FILE.name] =
    TESTING_NEGATIVE_SIZE_FILE;
  test_util.defaultMetadata['/' + TESTING_RELATIVE_NAME_FILE.name] =
    TESTING_RELATIVE_NAME_FILE;

  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      onOpenFileRequested);
  chrome.fileSystemProvider.onReadFileRequested.addListener(
      onReadFileRequested);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      onCloseFileRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Tests that returning a too big chunk (4 times larger than the file size,
    // and also much more than requested 1 KB of data).
    function returnTooLargeChunk() {
      test_util.fileSystem.root.getFile(
          TESTING_TOO_LARGE_CHUNK_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.file(chrome.test.callbackPass(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                chrome.test.fail('Reading should fail.');
              };
              fileReader.onerror = chrome.test.callbackPass();
              fileReader.readAsText(fileSlice);
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tests that calling a success callback with a non-existing request id
    // doesn't cause any harm.
    function invalidCallback() {
      test_util.fileSystem.root.getFile(
          TESTING_INVALID_CALLBACK_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.file(chrome.test.callbackPass(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                chrome.test.fail('Reading should fail.');
              };
              fileReader.onerror = chrome.test.callbackPass();
              fileReader.readAsText(fileSlice);
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Test that reading from files with negative size is not allowed.
    function negativeSize() {
      test_util.fileSystem.root.getFile(
          TESTING_NEGATIVE_SIZE_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.file(chrome.test.callbackPass(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = chrome.test.callbackPass(function(e) {
                var text = fileReader.result;
                chrome.test.assertEq(0, text.length);
              });
              fileReader.onerror = function(error) {
                chrome.test.fail(error.name);
              };
              fileReader.readAsText(fileSlice);
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tests that URLs generated from a file containing .. inside is properly
    // escaped.
    function relativeName() {
      test_util.fileSystem.root.getFile(
          TESTING_RELATIVE_NAME_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.fail('Opening a file should fail.');
          },
          chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('NotFoundError', error.name);
          }));
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
