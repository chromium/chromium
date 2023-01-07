// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testStartStopNotifications() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertEq(1, Object.keys(changedChrcs).length);
  chrome.test.assertEq(charId0, changedChrcs[charId0].instanceId);
  chrome.test.succeed();
}

var errorAlreadyNotifying = 'Already notifying';
var errorNotFound = 'Instance not found';
var errorNotNotifying = 'Not notifying';
var errorOperationFailed = 'Operation failed';
var errorPermissionDenied = 'Permission denied';

var charId0 = 'char_id0';
var charId1 = 'char_id1';
var charId2 = 'char_id2';

var changedChrcs = {};
var ble = chrome.bluetoothLowEnergy;
var start = ble.startCharacteristicNotifications;
var stop = ble.stopCharacteristicNotifications;


function earlyError(message) {
  error = message;
  chrome.test.runTests([testStartStopNotifications]);
}

function expectError(expectedMessage) {
  if (!chrome.runtime.lastError) {
    earlyError('Expected error: ' + expectedMessage);
  } else if (chrome.runtime.lastError.message != expectedMessage) {
    earlyError('Expected error: ' + expectedMessage + ', got error: ' +
        expectedMessage);
  }
  return error !== undefined;
}

function expectSuccess() {
  if (chrome.runtime.lastError) {
    earlyError('Unexpected error: ' + chrome.runtime.lastError.message);
  }
  return error !== undefined;
}

ble.onCharacteristicValueChanged.addListener(function (chrc) {
  changedChrcs[chrc.instanceId] = chrc;
});

start('foo', function () {
  if (expectError(errorNotFound))
    return;

  start(charId2, function () {
    if (expectError(errorPermissionDenied))
      return;

    stop(charId0, function () {
      if (expectError(errorNotNotifying))
        return;

      start(charId0, function () {
        if (expectError(errorOperationFailed))
          return;

        start(charId0, function () {
          if (expectSuccess())
            return;

          start(charId0, function () {
            if (expectError(errorAlreadyNotifying))
              return;

            start(charId1, function () {
              if (expectSuccess())
                return;

              stop(charId1, function () {
                if (expectSuccess())
                  return;

                stop(charId1, function () {
                  if (expectError(errorNotNotifying))
                    return;

                  stop(charId2, function () {
                    if (expectError(errorNotNotifying))
                      return;

                    chrome.test.sendMessage('ready', function (message) {
                      chrome.test.runTests([testStartStopNotifications]);
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
});
