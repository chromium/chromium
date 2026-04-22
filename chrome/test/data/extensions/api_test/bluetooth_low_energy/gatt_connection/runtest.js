// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const deviceAddress0 = '11:22:33:44:55:66';
const deviceAddress1 = '77:88:99:AA:BB:CC';
const badDeviceAddress = 'bad-address';
const ble = chrome.bluetoothLowEnergy;

const errorAlreadyConnected = 'Already connected';
const errorNotConnected = 'Not connected';
const errorNotFound = 'Instance not found';
const errorOperationFailed = 'Operation failed';
const errorAuthFailed = 'Authentication failed';
const errorInProgress = 'In progress';
const errorCanceled = 'Request canceled';
const errorTimeout = 'Operation timed out';
const errorUnsupportedDevice = 'This device is not supported on ' +
    'the current platform';
const errorNoMemory = 'No memory';
const errorJniEnvironment = 'JNI environment error';
const errorJniThreadAttach = 'JNI thread attach error';
const errorWakelock = 'Wakelock error';

function expectError(message) {
  if (!chrome.runtime.lastError) {
    chrome.test.fail('Expected error: <' + message + '> but there was none.');
  }
  if (chrome.runtime.lastError.message != message) {
    chrome.test.fail(
        'Expected error: <' + message + '> but it was: <' +
        chrome.runtime.lastError.message + '>');
  }
}

function expectSuccess() {
  if (chrome.runtime.lastError) {
    chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
  }
}

let queue = [];

function runNext() {
  if (queue.length == 0) {
    chrome.test.fail('No more tests!');
  }
  (queue.shift())();
}

function makeConnectErrorFunction(error) {
  return function() {
    expectError(error);

    ble.connect(deviceAddress0, runNext);
  };
}

queue = [
  function() {
    ble.disconnect(deviceAddress0, runNext);
  },
  function() {
    expectError(errorNotConnected);

    // Disconnect from deviceAddress1, (not connected)
    ble.disconnect(deviceAddress1, runNext);
  },
  function() {
    expectError(errorNotConnected);

    // Connect to device that doesn't exist.
    ble.connect(badDeviceAddress, runNext);
  },
  makeConnectErrorFunction(errorNotFound),
  makeConnectErrorFunction(errorOperationFailed),
  makeConnectErrorFunction(errorInProgress),
  makeConnectErrorFunction(errorAuthFailed),
  makeConnectErrorFunction(errorAuthFailed),
  makeConnectErrorFunction(errorCanceled),
  makeConnectErrorFunction(errorTimeout),
  makeConnectErrorFunction(errorUnsupportedDevice),
  makeConnectErrorFunction(errorNoMemory),
  makeConnectErrorFunction(errorJniEnvironment),
  makeConnectErrorFunction(errorJniThreadAttach),
  makeConnectErrorFunction(errorWakelock),
  function() {
    expectSuccess();

    // Device 0 already connected.
    ble.connect(deviceAddress0, runNext);
  },
  function() {
    expectError(errorAlreadyConnected);

    // Device 1 still disconnected.
    ble.disconnect(deviceAddress1, runNext);
  },
  function() {
    expectError(errorNotConnected);

    // Successful connect to device 1.
    ble.connect(deviceAddress1, runNext);
  },
  function() {
    expectSuccess();

    // Device 1 already connected.
    ble.connect(deviceAddress1, runNext);
  },
  function() {
    expectError(errorAlreadyConnected);

    // Successfully disconnect device 0.
    ble.disconnect(deviceAddress0, runNext);
  },
  function() {
    expectSuccess();

    // Cannot disconnect device 0.
    ble.disconnect(deviceAddress0, runNext);
  },
  function() {
    expectError(errorNotConnected);

    // Device 1 still connected.
    ble.connect(deviceAddress1, runNext);
  },
  function() {
    expectError(errorAlreadyConnected);

    // Successfully disconnect device 1.
    ble.disconnect(deviceAddress1, runNext);
  },
  function() {
    expectSuccess();

    // Cannot disconnect device 1.
    ble.disconnect(deviceAddress1, runNext);
  },
  function() {
    expectError(errorNotConnected);

    // Re-connect device 0.
    ble.connect(deviceAddress0, runNext);
  },
  function() {
    expectSuccess();
    chrome.test.succeed();
  },
];

runNext();
