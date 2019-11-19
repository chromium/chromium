// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const serial = chrome.serial;

// TODO(miket): opening Bluetooth ports on OSX is unreliable. Investigate.
function shouldSkipPort(portName) {
  return portName.match(/[Bb]luetooth/);
}

var createTestArrayBuffer = function(dataLength) {
  var buffer = new ArrayBuffer(dataLength);

  var uint8View = new Uint8Array(buffer);
  for (var i = 0; i < dataLength; i++) {
    uint8View[i] = (42 + i * 2) & 0xFF;  // An arbitrary pattern.
  }
  return buffer;
}

var testSerial = function() {
  var serialPort = null;
  var connectionId = -1;
  var receiveTries = 20;
  var sendBuffer = createTestArrayBuffer(8);
  var sendBufferUint8View = new Uint8Array(sendBuffer);
  var bufferLength = sendBufferUint8View.length;
  var receiveBuffer = new ArrayBuffer(bufferLength);
  var receiveBufferUint8View = new Uint8Array(receiveBuffer);
  var bytesToReceive = bufferLength;
  var has_read_error = false;
  var expectDisconnect = false;

  var operation = 0;
  var doNextOperation = function() {
    switch (operation++) {
      case 0:
        serial.getDevices(onGetDevices);
        break;
      case 1:
        serial.getConnections(onGetConnectionsEmpty);
        break;
      case 2:
        var bitrate = 57600;
        console.log(
            `Connecting to serial device ${serialPort} at ${bitrate} bps.`);
        serial.connect(serialPort, {bitrate: bitrate}, onConnect);
        break;
      case 3:
        serial.getConnections(onGetConnectionsOne);
        break;
      case 4:
        serial.setControlSignals(
            connectionId, {dtr: true}, onSetControlSignals);
        break;
      case 5:
        serial.getControlSignals(connectionId, onGetControlSignals);
        break;
      case 6:
        serial.onReceive.addListener(onReceive);
        serial.onReceiveError.addListener(onReceiveError);
        serial.send(connectionId, sendBuffer, onSend);
        break;
      case 7:
        sendBuffer = createTestArrayBuffer(16 * 1024);
        sendBufferUint8View = new Uint8Array(sendBuffer);
        bufferLength = sendBufferUint8View.length;
        receiveBuffer = new ArrayBuffer(bufferLength);
        receiveBufferUint8View = new Uint8Array(receiveBuffer);
        bytesToReceive = bufferLength;
        serial.send(connectionId, sendBuffer, onSend);
        break;
      case 8:
        expectDisconnect = true;
        serial.disconnect(connectionId, onDisconnect);
        break;
      case 9:
        // Wait for both onDisconnect() and onReceiveError().
        break;
      case 10:
        serial.getConnections(onGetConnectionsEmpty);
        break;
      case 11:
        var bitrate = 115200;
        console.log(
            `Reconnecting to serial device ${serialPort} at ${bitrate} bps.`);
        serial.connect(serialPort, {bitrate: bitrate}, onConnect);
        break;
      case 12:
        expectDisconnect = true;
        serial.disconnect(connectionId, onDisconnect);
        break;
      case 13:
        // Wait for both onDisconnect() and onReceiveError().
        break;
      case 14:
        serial.getConnections(onGetConnectionsEmpty);
        break;
      default:
        // Beware! If you forget to assign a case for your next test, the whole
        // test suite will appear to succeed!
        chrome.test.succeed();
        break;
    }
  };

  var skipToTearDown = function() {
    operation = 50;
    doNextOperation();
  };

  var repeatOperation = function() {
    operation--;
    doNextOperation();
  };

  var onGetConnectionsEmpty = function(connectionInfos) {
    chrome.test.assertEq(0, connectionInfos.length);
    doNextOperation();
  };

  var onGetConnectionsOne = function(connectionInfos) {
    chrome.test.assertEq(1, connectionInfos.length);
    chrome.test.assertEq(connectionId, connectionInfos[0].connectionId);
    doNextOperation();
  };

  var onDisconnect = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  var onReceive = function(receiveInfo) {
    var data = new Uint8Array(receiveInfo.data);
    bytesToReceive -= data.length;
    var receiveBufferIndex = bufferLength - bytesToReceive - data.length;
    for (var i = 0; i < data.length; i++)
      receiveBufferUint8View[i + receiveBufferIndex] = data[i];
    if (bytesToReceive == 0) {
      chrome.test.assertEq(sendBufferUint8View, receiveBufferUint8View,
                           'Buffer received was not equal to buffer sent.');
      if (!has_read_error) {
        doNextOperation();
      }
    } else if (--receiveTries <= 0) {
      chrome.test.fail('receive() failed to return requested number of bytes.');
    }
  };

  var onReceiveError = function(errorInfo) {
    chrome.test.assertEq(connectionId, errorInfo.connectionId,
                         "Unmatch connectionId for ReceiveError");
    if (expectDisconnect && errorInfo.error === "disconnected") {
      expectDisconnect = false;
      doNextOperation();
      return;
    }
    has_read_error = true;
    if (errorInfo.error == "parity_error") {
      serial.getInfo(connectionId, onGetInfoToReconnect);
    } else {
      chrome.test.fail('Failed to receive serial data: ' + errorInfo.error);
    }
  };

  var onGetInfoToReconnect = function(connectionInfo) {
    chrome.test.assertEq(true, connectionInfo.paused,
                         'Failed to pause connection on read error.');
    // Try to reconnect the read data pipe.
    serial.setPaused(connectionId, false, () => {
      serial.getInfo(connectionId, onGetInfoAfterReconnect);
    });
  };

  var onGetInfoAfterReconnect = function(connectionInfo) {
    if (connectionInfo.paused != false) {
      chrome.test.fail('Failed to reconnect on read error.');
    }
    has_read_error = false;
    doNextOperation();
  };

  var onSend = function(sendInfo) {
    chrome.test.assertEq(bufferLength, sendInfo.bytesSent,
                         'Failed to send byte.');
  };

  var onGetControlSignals = function(options) {
    chrome.test.assertTrue(typeof options.dcd != 'undefined', "No DCD set");
    chrome.test.assertTrue(typeof options.cts != 'undefined', "No CTS set");
    chrome.test.assertTrue(typeof options.ri != 'undefined', "No RI set");
    chrome.test.assertTrue(typeof options.dsr != 'undefined', "No DSR set");
    doNextOperation();
  };

  var onSetControlSignals = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  var onConnect = function(connectionInfo) {
    chrome.test.assertTrue(!!connectionInfo,
                           'Failed to connect to serial port.');
    connectionId = connectionInfo.connectionId;
    doNextOperation();
  };

  var onGetDevices = function(devices) {
    if (devices.length > 0) {
      var portNumber = 0;
      while (portNumber < devices.length) {
        if (shouldSkipPort(devices[portNumber].path)) {
          portNumber++;
          continue;
        } else
          break;
      }
      if (portNumber < devices.length) {
        serialPort = devices[portNumber].path;
        doNextOperation();
      } else {
        // We didn't find a port that we think we should try.
        chrome.test.succeed();
      }
    } else {
      // No serial device found. This is still considered a success because we
      // can't rely on specific hardware being present on the machine.
      chrome.test.succeed();
    }
  };

  doNextOperation();
};

chrome.test.runTests([testSerial]);
