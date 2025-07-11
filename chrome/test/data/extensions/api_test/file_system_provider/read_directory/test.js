// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let testUtil;

/**
 * @type {Object}
 * @const
 */
var TESTING_HELLO_DIR = Object.freeze({
  isDirectory: true,
  name: 'hello'
});

/**
 * @type {Object}
 * @const
 */
var TESTING_CANDIES_DIR = Object.freeze({
  isDirectory: true,
  name: 'candies'
});

/**
 * @type {Object}
 * @const
 */
var TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt'
});

/**
 * Returns entries in the requested directory.
 *
 * @param {ReadDirectoryRequestedOptions} options Options.
 * @param {function(Array<Object>, boolean)} onSuccess Success callback with
 *     a list of entries. May be called multiple times.
 * @param {function(string)} onError Error callback with an error code.
 */
function onReadDirectoryRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== testUtil.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.directoryPath !== '/' + TESTING_HELLO_DIR.name) {
    onError('NOT_FOUND');  // enum ProviderError.
    return;
  }

  onSuccess([TESTING_TIRAMISU_FILE], true /* hasMore */);
  onSuccess([TESTING_CANDIES_DIR], false /* hasMore */);
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      testUtil.onGetMetadataRequestedDefault);

  testUtil.defaultMetadata['/' + TESTING_HELLO_DIR.name] =
      TESTING_HELLO_DIR;
  testUtil.defaultMetadata['/' + TESTING_HELLO_DIR.name + '/' +
        TESTING_TIRAMISU_FILE.name] = TESTING_TIRAMISU_FILE;
  testUtil.defaultMetadata['/' + TESTING_HELLO_DIR.name + '/' +
      TESTING_CANDIES_DIR.name] = TESTING_CANDIES_DIR;

  chrome.fileSystemProvider.onReadDirectoryRequested.addListener(
      onReadDirectoryRequested);

  testUtil.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Read contents of the /hello directory. This directory exists, so it
    // should succeed.
    function readEntriesSuccess() {
      testUtil.fileSystem.root.getDirectory(
          'hello',
          {create: false},
          chrome.test.callbackPass(function(dirEntry) {
            var dirReader = dirEntry.createReader();
            var entries = [];
            var readEntriesNext = function() {
              dirReader.readEntries(
                  chrome.test.callbackPass(function(inEntries) {
                    Array.prototype.push.apply(entries, inEntries);
                    if (!inEntries.length) {
                      // No more entries, so verify.
                      chrome.test.assertEq(2, entries.length);
                      chrome.test.assertTrue(entries[0].isFile);
                      chrome.test.assertEq('tiramisu.txt', entries[0].name);
                      chrome.test.assertEq(
                          '/hello/tiramisu.txt', entries[0].fullPath);
                      chrome.test.assertTrue(entries[1].isDirectory);
                      chrome.test.assertEq('candies', entries[1].name);
                      chrome.test.assertEq(
                          '/hello/candies', entries[1].fullPath);
                    } else {
                      readEntriesNext();
                    }
                  }), function(error) {
                    chrome.test.fail();
                  });
            };
            readEntriesNext();
          }),
          function(error) {
            chrome.test.fail();
          });
    },
    // Read contents of a directory which does not exist, what should return an
    // error.
    function readEntriesError() {
      testUtil.fileSystem.root.getDirectory(
          'cranberries',
          {create: false},
          function(dirEntry) {
            chrome.test.fail();
          },
          chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('NotFoundError', error.name);
          }));
    }
  ]);
}

// This works-around that background scripts can't import because they aren't
// considered modules.
(async () => {
  testUtil = await import(
    '/_test_resources/api_test/file_system_provider/test_util.js');

  // Setup and run all of the test cases.
  setUp(runTests);
})();
