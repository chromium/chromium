// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Testing contents for files.
 * @type {string}
 * @const
 */
var TESTING_TEXT = 'I have a basket full of fruits.';

/**
 * Metadata of a healthy file used to read contents from.
 * @type {Object}
 * @const
 */
var TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: TESTING_TEXT.length,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata of a broken file used to read contents from.
 * @type {Object}
 * @const
 */
var TESTING_BROKEN_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'broken-tiramisu.txt',
  size: TESTING_TEXT.length,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata of a broken file used to read contents from, but it simulates
 * a very long read, in order to verify the aborting mechanism.
 * @type {Object}
 * @const
 */
var TESTING_VANILLA_FOR_ABORT_FILE = Object.freeze({
  isDirectory: false,
  name: 'vanilla.txt',
  size: TESTING_TEXT.length,
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Read breakpoint callback invoked when reading some testing files.
 * The first argument is a file path, and the second one is a callback to resume
 * reading the file.
 *
 * @type {?function(string, function()}
 */
var readBreakpointCallback = null;

/**
 * Open breakpoint callback invoked when opening some testing files.
 * The first argument is a file path, and the second one is a callback to resume
 * opening the file.
 *
 * @type {?function(string, function()}
 */
var openBreakpointCallback = null;

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
  var filePath = test_util.openedFiles[options.openRequestId];

  var continueRead = function() {
    if (options.fileSystemId !== test_util.FILE_SYSTEM_ID || !filePath) {
      onError('SECURITY');  // enum ProviderError.
      return;
    }

    if (filePath === '/' + TESTING_TIRAMISU_FILE.name) {
      var textToSend = TESTING_TEXT.substr(options.offset, options.length);
      var textToSendInChunks = textToSend.split(/(?= )/);

      textToSendInChunks.forEach((item, index) => {
        // Convert item (string) to an ArrayBuffer.
        onSuccess(
            /*data=*/new TextEncoder().encode(item).buffer,
            /*hasMore=*/index < textToSendInChunks.length - 1);
      });
    }

    if (filePath === '/' + TESTING_VANILLA_FOR_ABORT_FILE.name) {
      // Do nothing. This simulates a very slow read.
      return;
    }

    if (filePath === '/' + TESTING_BROKEN_TIRAMISU_FILE.name) {
      onError('ACCESS_DENIED');  // enum ProviderError.
      return;
    }

    onError('INVALID_OPERATION');  // enum ProviderError.
  };

  if (readBreakpointCallback)
    readBreakpointCallback(filePath, continueRead);
  else
    continueRead();
}

/**
 * Handles opening files. Further file operations will be associated with the
 * <code>requestId</code>.
 *
 * @param {OpenFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onOpenFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  var continueOpen = function() {
    var metadata = test_util.defaultMetadata[options.filePath];
    if (metadata && !metadata.is_directory) {
      test_util.openedFiles[options.requestId] = options.filePath;
      onSuccess();
    } else {
      onError('NOT_FOUND');  // enum ProviderError.
    }
  };

  if (openBreakpointCallback)
    openBreakpointCallback(options.filePath, continueOpen);
  else
    continueOpen();
};

/**
 * Sets up the tests. Called once per all test cases. For each test case,
 * setUpFileSystem() must to be called with additional, test-case specific
 * options.
 */
function setUp() {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      test_util.onCloseFileRequested);

  test_util.defaultMetadata['/' + TESTING_TIRAMISU_FILE.name] =
      TESTING_TIRAMISU_FILE;
  test_util.defaultMetadata['/' + TESTING_BROKEN_TIRAMISU_FILE.name] =
      TESTING_BROKEN_TIRAMISU_FILE;
  test_util.defaultMetadata['/' + TESTING_VANILLA_FOR_ABORT_FILE.name] =
      TESTING_VANILLA_FOR_ABORT_FILE;

  chrome.fileSystemProvider.onReadFileRequested.addListener(
      onReadFileRequested);
  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      onOpenFileRequested);
}

/**
 * Sets up a testing provided file system. If it was previously mounted, then
 * unmounts it first. In case of an error, fails with an assert.
 *
 * @param {number} openedFilesLimit Limit of opened files at once. If 0, then
 *     not limited.
 * @param {function()} callback Completion callback.
 */
function setUpFileSystem(openedFilesLimit, callback) {
  var options = {};
  if (openedFilesLimit)
    options.openedFilesLimit = openedFilesLimit;
  // TODO(mtomasz): Rather than clearing out opened files tests should wait for
  // all files to be closed before unmounting the file system. crbug.com/789083
  test_util.openedFiles = [];
  if (test_util.fileSystem) {
    chrome.fileSystemProvider.unmount({
      fileSystemId: test_util.FILE_SYSTEM_ID
    }, chrome.test.callbackPass(function() {
      test_util.mountFileSystem(callback, options);
    }));
  } else {
    test_util.mountFileSystem(callback, options);
  }
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Read contents of the /tiramisu.txt file. This file exists, so it should
    // succeed.
    function readFileSuccess() {
      setUpFileSystem(0 /* no limit */, chrome.test.callbackPass(function() {
        test_util.fileSystem.root.getFile(
            TESTING_TIRAMISU_FILE.name,
            {create: false},
            chrome.test.callbackPass(function(fileEntry) {
              fileEntry.file(chrome.test.callbackPass(function(file) {
                var fileReader = new FileReader();
                fileReader.onload = chrome.test.callbackPass(function(e) {
                  var text = fileReader.result;
                  chrome.test.assertEq(TESTING_TEXT, text);
                });
                fileReader.onerror = function(e) {
                  chrome.test.fail(fileReader.error.name);
                };
                fileReader.readAsText(file);
              }),
              function(error) {
                chrome.test.fail(error.name);
              });
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
          }));
    },

    // Read contents of the /tiramisu.txt multiple times at once. Verify that
    // there is at most as many opened files at once as permitted per limit.
    function readFileWithOpenedFilesLimitSuccess() {
      setUpFileSystem(2 /* two files */, chrome.test.callbackPass(function() {
        var initAllReadsPromise;

        // Set a breakpoint on reading a file, and continue once another file
        // is queued.
        readBreakpointCallback = function(filePath, continueCallback) {
          chrome.test.assertEq('/' + TESTING_TIRAMISU_FILE.name, filePath);
          // Continue after all reads are initiated.
          initAllReadsPromise.then(chrome.test.callbackPass(function() {
            chrome.test.assertTrue(
                Object.keys(test_util.openedFiles).length <= 2);
            continueCallback();
          })).catch(function(error) {
            chrome.test.fail(error.rname);
          });
        };

        // Initiate reads, but all of them will be stoped on a breakpoint on
        // the first read.
        var initReadPromises = [];
        for (var i = 0; i < 16; i++) {
          initReadPromises.push(new Promise(
            chrome.test.callbackPass(function(fulfill) {
              test_util.fileSystem.root.getFile(
                  TESTING_TIRAMISU_FILE.name,
                  {create: false},
                  chrome.test.callbackPass(function(fileEntry) {
                    fileEntry.file(chrome.test.callbackPass(function(file) {
                      var fileReader = new FileReader();
                      fileReader.onload = chrome.test.callbackPass(function(e) {
                        var text = fileReader.result;
                        chrome.test.assertEq(TESTING_TEXT, text);
                      });
                      fileReader.onerror = function(e) {
                        chrome.test.fail(fileReader.error.name);
                      };
                      fileReader.readAsText(file);
                      fulfill();
                    }),
                    function(error) {
                      chrome.test.fail(error.name);
                    });
                  }),
                  function(error) {
                    chrome.test.fail(error.name);
                  });
            })));
        }

        initAllReadsPromise = Promise.all(initReadPromises);
      }));
    },

    // Read contents of a file,  but with an error on the way. This should
    // result in an error.
    function readEntriesError() {
      setUpFileSystem(0 /* no limit */, chrome.test.callbackPass(function() {
        // Reset the breakpoint from the previous test case.
        readBreakpointCallback = null;
        test_util.fileSystem.root.getFile(
            TESTING_BROKEN_TIRAMISU_FILE.name,
            {create: false},
            chrome.test.callbackPass(function(fileEntry) {
              fileEntry.file(chrome.test.callbackPass(function(file) {
                var fileReader = new FileReader();
                fileReader.onload = function(e) {
                  chrome.test.fail();
                };
                fileReader.onerror = chrome.test.callbackPass(function(e) {
                  chrome.test.assertEq(
                      'NotReadableError', fileReader.error.name);
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
        }));
    },

    // Abort reading a file with a registered abort handler. Should result in a
    // gracefully terminated reading operation.
    function abortReadingSuccess() {
      setUpFileSystem(0 /* no limit */, chrome.test.callbackPass(function() {
        var onAbortRequested = chrome.test.callbackPass(
            function(options, onSuccess, onError) {
              onSuccess();
              chrome.fileSystemProvider.onAbortRequested.removeListener(
                  onAbortRequested);
            });

        chrome.fileSystemProvider.onAbortRequested.addListener(
            onAbortRequested);

        test_util.fileSystem.root.getFile(
            TESTING_VANILLA_FOR_ABORT_FILE.name,
            {create: false, exclusive: false},
            chrome.test.callbackPass(function(fileEntry) {
              fileEntry.file(chrome.test.callbackPass(function(file) {
                var fileReader = new FileReader();
                fileReader.onabort = chrome.test.callbackPass(function(e) {
                  chrome.test.assertEq(
                      'AbortError', fileReader.error.name);
                });
                // Set a breakpoint on reading a file, so aborting is invoked
                // after it's started.
                readBreakpointCallback = function(filePath, continueCallback) {
                  fileReader.abort();
                };
                fileReader.readAsText(file);
              }),
              function(error) {
                chrome.test.fail(error.name);
              });
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
      }));
    },

    // Abort opening a file while trying to read it without an abort handler
    // wired up. This should cause closing the file anyway.
    function abortViaCloseSuccess() {
      setUpFileSystem(0 /* no limit */, chrome.test.callbackPass(function() {
        test_util.fileSystem.root.getFile(
            TESTING_VANILLA_FOR_ABORT_FILE.name,
            {create: false, exclusive: false},
            chrome.test.callbackPass(function(fileEntry) {
              fileEntry.file(chrome.test.callbackPass(function(file) {
                var fileReader = new FileReader();
                fileReader.onabort = chrome.test.callbackPass(function(e) {
                  chrome.test.assertEq(
                      'AbortError', fileReader.error.name);
                  // Confirm that the file is closed on the provider side.
                  chrome.test.assertEq(
                      0, Object.keys(test_util.openedFiles).length);
                });
                // Set a breakpoint on reading a file, so aborting is invoked
                // after it's started.
                openBreakpointCallback = chrome.test.callbackPass(
                    function(filePath, continueCallback) {
                      fileReader.abort();
                      setTimeout(chrome.test.callbackPass(function() {
                        continueCallback();
                        chrome.test.assertEq(
                            1, Object.keys(test_util.openedFiles).length);
                      }), 0);
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
      }));
    },

    // Abort opening a file while trying to read it without an abort handler
    // wired up, then quickly try to open it again while having a limit of 1
    // opened files at once. This is a regression test for: crbug.com/519063.
    function abortOpenedAndReopenSuccess() {
      setUpFileSystem(1 /* no limit */, chrome.test.callbackPass(function() {
        test_util.fileSystem.root.getFile(
            TESTING_VANILLA_FOR_ABORT_FILE.name,
            {create: false, exclusive: false},
            chrome.test.callbackPass(function(fileEntry) {
              fileEntry.file(chrome.test.callbackPass(function(file) {
                var fileReader = new FileReader();
                var fileReader2 = new FileReader();
                fileReader.onabort = chrome.test.callbackPass(function(e) {
                  chrome.test.assertEq(
                      'AbortError', fileReader.error.name);
                  // Confirm that the file is closed on the provider side.
                  chrome.test.assertEq(
                      0, Object.keys(test_util.openedFiles).length);
                });
                // Set a breakpoint on reading a file, so aborting is invoked
                // after it's started.
                openBreakpointCallback = chrome.test.callbackPass(
                    function(filePath, continueCallback) {
                      fileReader.abort();
                      setTimeout(chrome.test.callbackPass(function() {
                        continueCallback();
                        chrome.test.assertEq(
                            1, Object.keys(test_util.openedFiles).length);
                      }), 0);
                      openBreakpointCallback = chrome.test.callbackPass(
                          function() {
                            // The next OpenFile request should happen only
                            // after the previous file is closed successfully
                            // due to abort.
                            chrome.test.assertEq(
                                0, Object.keys(test_util.openedFiles).length);
                          });
                    });
                fileReader.readAsText(file);
                // The second reader should enqueue until the first file is
                // closed.
                fileReader2.readAsText(file);
              }),
              function(error) {
                chrome.test.fail(error.name);
              });
            }),
            function(error) {
              chrome.test.fail(error.name);
            });
      }));
    },

  ]);
}

// Setup and run all of the test cases.
setUp();
runTests();
