// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let error;

function testReadDescriptorValue() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, descriptor, '\'descriptor\' is null');
  chrome.test.assertEq(descId, descriptor.instanceId);

  chrome.test.succeed();
}

const readDescriptorValue = chrome.bluetoothLowEnergy.readDescriptorValue;
var descId = 'desc_id0';
const badDescId = 'desc_id1';

var descriptor = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testReadDescriptorValue]);
}

let queue = [];

function runNext(result) {
  if (queue.length == 0) {
    chrome.test.fail('No more tests!');
  }

  (queue.shift())(result);
}

const errorAuthenticationFailed = 'Authentication failed';
const errorCanceled = 'Request canceled';
const errorFailed = 'Operation failed';
const errorGattNotSupported = 'Operation not supported by this service';
const errorHigherSecurity = 'Higher security needed';
const errorInProgress = 'In progress';
const errorInsufficientAuthorization = 'Insufficient authorization';
const errorInvalidLength = 'Invalid attribute value length';
const errorNotConnected = 'Not connected';
const errorNotFound = 'Instance not found';
const errorNotNotifying = 'Not notifying';
const errorOperationFailed = 'Operation failed';
const errorPermissionDenied = 'Permission denied';
const errorTimeout = 'Operation timed out';
const errorUnsupportedDevice =
    'This device is not supported on the current platform';
const errorPlatformNotSupported =
    'This operation is not supported on the current platform';

function makeExpectedErrorCallback(expectedError) {
  return function(result) {
    console.log('Expecting error ' + expectedError);
    if (result || !chrome.runtime.lastError ||
        chrome.runtime.lastError.message != expectedError) {
      errorMsg = 'readDescriptorValue expected error \'' + expectedError + '\'';
      if (chrome.runtime.lastError) {
        errorMsg =
            errorMsg + ' but got \'' + chrome.runtime.lastError.message + '\'';
      }
      earlyError(errorMsg);
      return;
    }

    readDescriptorValue(descId, runNext);
  };
}

queue = [
  function() {
    // 1. Unknown descriptor instanceId.
    readDescriptorValue(badDescId, runNext);
  },
  function(result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('\'badDescId\' did not cause failure');
      return;
    }

    // 2. Known descriptor instanceId, but call failure.
    readDescriptorValue(descId, runNext);
  },
  makeExpectedErrorCallback(errorFailed),
  makeExpectedErrorCallback(errorInvalidLength),
  makeExpectedErrorCallback(errorPermissionDenied),
  makeExpectedErrorCallback(errorInsufficientAuthorization),
  makeExpectedErrorCallback(errorHigherSecurity),
  makeExpectedErrorCallback(errorGattNotSupported),
  makeExpectedErrorCallback(errorInProgress),
  function(result) {
    if (chrome.runtime.lastError) {
      earlyError(chrome.runtime.lastError.message);
      return;
    }

    descriptor = result;

    chrome.test.sendMessage('ready', function(message) {
      chrome.test.runTests([testReadDescriptorValue]);
    });
  },
];

runNext();
