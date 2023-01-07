// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress = '11:12:13:14:15:16';
var errorNotConnected = 'Device not connected';
var errorDisconnectFailed = 'Failed to disconnect device';

var btp = chrome.bluetoothPrivate;

function testDisconnect() {
  btp.disconnectAll(deviceAddress, function() {
    assertFailure(errorNotConnected);
    btp.disconnectAll(deviceAddress, function() {
      assertFailure(errorNotConnected);
      btp.disconnectAll(deviceAddress, function() {
        assertFailure(errorDisconnectFailed);
        btp.disconnectAll(deviceAddress, function() {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
      });
    });
  });
}

function assertFailure(message) {
  if (!chrome.runtime.lastError)
    chrome.test.fail('Expected failure but got success.');

  if (chrome.runtime.lastError.message == message)
    return;

  chrome.test.fail('Expected error "' + message + '" but got "' +
                   chrome.runtime.lastError.message + '" instead.');
}

chrome.test.runTests([testDisconnect]);
