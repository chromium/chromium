// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Metadata of a healthy file used to read contents from.
 * @type {Object}
 * @const
 */
var TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: 1337,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Callback called when reading of a file is invoked.
 * @type {?function()}
 */
var readBreakpointCallback = null;

/**
 * Handles reading, but never calls a callback in order to keep the file opened
 * for tests.
 *
 * @param {ReadFileRequestedOptions} options Options.
 * @param {function(ArrayBuffer, boolean)} onSuccess Success callback with a
 *     chunk of data, and information if more data will be provided later.
 * @param {function(string)} onError Error callback.
 */
function onReadFileRequested(options, onSuccess, onError) {
  readBreakpointCallback();
  // Do not invoke any callback to avoid closing the file.
}

/**
 * Sets up the tests. Called once per all test cases. For each test case,
 * setUpFileSystem() must to be called with additional, test-case specific
 * options.
 */
function setUp() {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      test_util.onOpenFileRequested);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      test_util.onCloseFileRequested);

  test_util.defaultMetadata['/' + TESTING_TIRAMISU_FILE.name] =
      TESTING_TIRAMISU_FILE;

  chrome.fileSystemProvider.onReadFileRequested.addListener(
      onReadFileRequested);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Verifies if getAll() returns the mounted file system.
    function mountSuccess() {
      test_util.mountFileSystem(
          chrome.test.callbackPass(function() {
            // Start reading a file in order to open it. Note, that there is no
            // way to directly open a file from JavaScript.
            test_util.fileSystem.root.getFile(
                TESTING_TIRAMISU_FILE.name,
                {create: false},
                chrome.test.callbackPass(function(fileEntry) {
                  fileEntry.file(chrome.test.callbackPass(function(file) {
                    var fileReader = new FileReader();
                    readBreakpointCallback = chrome.test.callbackPass(
                        function() {
                          chrome.fileSystemProvider.getAll(
                              chrome.test.callbackPass(function(fileSystems) {
                                chrome.test.assertEq(1, fileSystems.length);
                                chrome.test.assertEq(
                                    test_util.FILE_SYSTEM_ID,
                                    fileSystems[0].fileSystemId);
                                chrome.test.assertEq(
                                    test_util.FILE_SYSTEM_NAME,
                                    fileSystems[0].displayName);
                                chrome.test.assertTrue(
                                    fileSystems[0].writable);
                                chrome.test.assertEq(2,
                                    fileSystems[0].openedFilesLimit);
                                chrome.test.assertEq(
                                    1, fileSystems[0].openedFiles.length);
                                chrome.test.assertEq(
                                    '/' + TESTING_TIRAMISU_FILE.name,
                                    fileSystems[0].openedFiles[0].filePath);
                                chrome.test.assertEq(
                                    chrome.fileSystemProvider.OpenFileMode.READ,
                                    fileSystems[0].openedFiles[0].mode);
                              }));

                          chrome.fileSystemProvider.get(
                              test_util.FILE_SYSTEM_ID,
                              chrome.test.callbackPass(function(fileSystem) {
                                chrome.test.assertEq(
                                    test_util.FILE_SYSTEM_ID,
                                    fileSystem.fileSystemId);
                                chrome.test.assertEq(
                                    test_util.FILE_SYSTEM_NAME,
                                    fileSystem.displayName);
                                chrome.test.assertTrue(fileSystem.writable);
                                chrome.test.assertEq(2,
                                    fileSystem.openedFilesLimit);
                                chrome.test.assertEq(
                                    1, fileSystem.openedFiles.length);
                                chrome.test.assertEq(
                                    '/' + TESTING_TIRAMISU_FILE.name,
                                    fileSystem.openedFiles[0].filePath);
                                chrome.test.assertEq(
                                    chrome.fileSystemProvider.OpenFileMode.READ,
                                    fileSystem.openedFiles[0].mode);
                                }));
                        });
                    fileReader.readAsText(file);
                  }),
                  function(error) {
                    chrome.test.fail(error.name);
                  });
                }),
                function(error) {
                  chrome.test.fail(error.name);
                });
          }), {writable: true, openedFilesLimit: 2});
    },

    // Verifies that after unmounting, the file system is not available in
    // getAll() list.
    function unmountSuccess() {
      chrome.fileSystemProvider.unmount(
          {fileSystemId: test_util.FILE_SYSTEM_ID},
          chrome.test.callbackPass(function() {
            chrome.fileSystemProvider.getAll(chrome.test.callbackPass(
                function(fileSystems) {
                  chrome.test.assertEq(0, fileSystems.length);
                }));
            chrome.fileSystemProvider.get(
                test_util.FILE_SYSTEM_ID,
                chrome.test.callbackFail('NOT_FOUND'));
          }));
    },

    // Verifies that if mounting fails, then the file system is not added to the
    // getAll() list.
    function mountError() {
      chrome.fileSystemProvider.mount({
        fileSystemId: '',
        displayName: ''
      }, chrome.test.callbackFail('INVALID_OPERATION', function() {
        chrome.fileSystemProvider.getAll(chrome.test.callbackPass(
            function(fileSystems) {
              chrome.test.assertEq(0, fileSystems.length);
            }));
        chrome.fileSystemProvider.get(
            test_util.FILE_SYSTEM_ID,
            chrome.test.callbackFail('NOT_FOUND'));
      }));
    }
  ]);
}

// Setup and run all of the test cases.
setUp();
runTests();
