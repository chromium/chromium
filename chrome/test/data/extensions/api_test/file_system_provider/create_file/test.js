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
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_NEW_FILE = Object.freeze({
  isDirectory: false,
  name: 'puppy',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onCreateFileRequested.addListener(
      test_util.onCreateFileRequested);

  test_util.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Create a file which doesn't exist. Should succeed.
    function createFileSuccessSimple() {
      test_util.fileSystem.root.getFile(
          TESTING_NEW_FILE.name, {create: true},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_NEW_FILE.name, entry.name);
            chrome.test.assertFalse(entry.isDirectory);
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Create a file which exists, non-exclusively. Should succeed.
    function createFileOrOpenSuccess() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: true, exclusive: false},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_FILE.name, entry.name);
            chrome.test.assertFalse(entry.isDirectory);
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Create a file which exists, exclusively. Should fail.
    function createFileExistsError() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name, {create: true, exclusive: true},
          function(entry) {
            chrome.test.fail('Created a file, but should fail.');
          }, chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('InvalidModificationError', error.name);
          }));
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
