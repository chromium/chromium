// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string}
 * @const
 */
var FIRST_FILE_SYSTEM_ID = 'vanilla';

/**
 * @type {string}
 * @const
 */
var SECOND_FILE_SYSTEM_ID = 'ice-cream';

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  Promise.all([
    new Promise(function(fulfill, reject) {
      chrome.fileSystemProvider.mount(
          {fileSystemId: FIRST_FILE_SYSTEM_ID, displayName: 'vanilla.zip'},
          chrome.test.callbackPass(fulfill));
    }),
    new Promise(function(fulfill, reject) {
      chrome.fileSystemProvider.mount(
          {fileSystemId: SECOND_FILE_SYSTEM_ID, displayName: 'ice-cream.zip'},
          chrome.test.callbackPass(fulfill));
    })
  ]).then(callback).catch(function(error) {
    chrome.test.fail(error.stack || error);
  });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Tests the fileSystemProvider.unmount(). Verifies if the unmount event
    // is emitted by VolumeManager.
    function unmount() {
      var onMountCompleted = function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('success', event.status);
        // For extension based providers, provider id is the same as
        // extension id.
        chrome.test.assertEq(
            chrome.runtime.id, event.volumeMetadata.providerId);
        chrome.test.assertEq(
            FIRST_FILE_SYSTEM_ID, event.volumeMetadata.fileSystemId);
        chrome.fileManagerPrivate.onMountCompleted.removeListener(
            onMountCompleted);
      };

      chrome.fileManagerPrivate.onMountCompleted.addListener(
          onMountCompleted);
      chrome.fileSystemProvider.unmount(
          {fileSystemId: FIRST_FILE_SYSTEM_ID},
          chrome.test.callbackPass());
    },

    // Tests the fileSystemProvider.unmount() with a wrong id. Verifies that
    // it fails with a correct error code.
    function unmountWrongId() {
      chrome.fileSystemProvider.unmount(
          {fileSystemId: 'wrong-fs-id'},
          chrome.test.callbackFail('NOT_FOUND'));
    },

    // Tests if fileManagerPrivate.removeMount() for provided file systems emits
    // the onMountRequested() event with correct arguments.
    function requestUnmountSuccess() {
      var onUnmountRequested = function(options, onSuccess, onError) {
        chrome.test.assertEq(SECOND_FILE_SYSTEM_ID, options.fileSystemId);
        // Not calling fileSystemProvider.unmount(), so the onMountCompleted
        // event will not be raised.
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
        onSuccess();
        chrome.test.succeed();
      };

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);

      test_util.getVolumeInfo(SECOND_FILE_SYSTEM_ID, function(volumeInfo) {
        chrome.test.assertTrue(!!volumeInfo);
        chrome.fileManagerPrivate.removeMount(volumeInfo.volumeId, () => {
          chrome.test.assertNoLastError();
        });
      });
    },

    // End to end test with a failure. Invokes fileSystemProvider.removeMount()
    // on a provided file system, and verifies (1) if the onMountRequested()
    // event is called with correct aguments, and (2) if calling onError(),
    // results in an unmount event fired from the VolumeManager instance.
    function requestUnmountError() {
      var unmountRequested = false;

      var onUnmountRequested = function(options, onSuccess, onError) {
        chrome.test.assertEq(false, unmountRequested);
        chrome.test.assertEq(SECOND_FILE_SYSTEM_ID, options.fileSystemId);
        onError('IN_USE');  // enum ProviderError.
        unmountRequested = true;
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
      };

      var onMountCompleted = chrome.test.callbackPass(function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('unknown_error', event.status);
        // For extension based providers, provider id is the same as
        // extension id.
        chrome.test.assertEq(
            chrome.runtime.id, event.volumeMetadata.providerId);
        chrome.test.assertEq(
            SECOND_FILE_SYSTEM_ID, event.volumeMetadata.fileSystemId);
        chrome.test.assertTrue(unmountRequested);

        // Remove the handlers and mark the test as succeeded.
        chrome.fileManagerPrivate.removeMount(SECOND_FILE_SYSTEM_ID, () => {});
        chrome.fileManagerPrivate.onMountCompleted.removeListener(
            onMountCompleted);
      });

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);
      chrome.fileManagerPrivate.onMountCompleted.addListener(onMountCompleted);

      test_util.getVolumeInfo(SECOND_FILE_SYSTEM_ID, function(volumeInfo) {
        chrome.test.assertTrue(!!volumeInfo);
        chrome.fileManagerPrivate.removeMount(volumeInfo.volumeId, () => {
          chrome.test.assertNoLastError();
        });
      });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
