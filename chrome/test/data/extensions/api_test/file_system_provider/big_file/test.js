// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {DOMFileSystem}
 */
var fileSystem = null;

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object<number, string>}
 */
var openedFiles = {};

/**
 * @type {string}
 * @const
 */
var TESTING_TEXT = 'We are bytes at the 5th GB of the file!';

/**
 * @type {number}
 * @const
 */
var TESTING_TEXT_OFFSET = 5 * 1000 * 1000 * 1000;

/**
 * Metadata for a testing file with 6GB file size.
 * @type {Object}
 * @const
 */
var TESTING_6GB_FILE = Object.freeze({
  isDirectory: false,
  name: '6gb.txt',
  size: 6 * 1024 * 1024 * 1024,
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

  if (options.filePath !== '/' + TESTING_6GB_FILE.name) {
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

  if (filePath === '/' + TESTING_6GB_FILE.name) {
    if (options.offset < TESTING_TEXT_OFFSET ||
        options.offset >= TESTING_TEXT_OFFSET + TESTING_TEXT.length) {
      console.error('Reading from a wrong location in the file!');
      onError('FAILED');  // enum ProviderError.
      return;
    }

    var buffer = TESTING_TEXT.substr(
        options.offset - TESTING_TEXT_OFFSET, options.length);
    var reader = new FileReader();
    reader.onload = function(e) {
      onSuccess(e.target.result, false /* hasMore */);
    };
    reader.readAsArrayBuffer(new Blob([buffer]));
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

  test_util.defaultMetadata['/' + TESTING_6GB_FILE.name] =
      TESTING_6GB_FILE;

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
    // Read contents of a new file, which is 6GB in size. Such size does not
    // fit in a 32bit int nor in size_t (unsigned). It should be safe to assume,
    // that if 6GB are supported, then there is no 32bit bottle neck, and the
    // next one would be 64bit. File System Provider API should support files
    // with size greater or equal to 2^53.
    function readBigFileSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_6GB_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            fileEntry.file(chrome.test.callbackPass(function(file) {
              // Read 10 bytes from the 5th GB.
              var fileSlice =
                  file.slice(TESTING_TEXT_OFFSET,
                             TESTING_TEXT_OFFSET + TESTING_TEXT.length);
              var fileReader = new FileReader();
              fileReader.onload = chrome.test.callbackPass(function(e) {
                var text = fileReader.result;
                chrome.test.assertEq(TESTING_TEXT, text);
              });
              fileReader.onerror = function(e) {
                chrome.test.fail(fileReader.error.name);
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
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
