// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const serial = chrome.serial;

// TODO(miket): opening Bluetooth ports on OSX is unreliable. Investigate.
function shouldSkipPort(portName) {
  return portName.match(/[Bb]luetooth/);
}

const createTestArrayBuffer = function(dataLength) {
  const buffer = new ArrayBuffer(dataLength);

  const uint8View = new Uint8Array(buffer);
  for (let i = 0; i < dataLength; i++) {
    uint8View[i] = (42 + i * 2) & 0xFF;  // An arbitrary pattern.
  }
  return buffer;
};

const testSerial = function() {
  let serialPort = null;
  let connectionId = -1;
  let receiveTries = 20;
  let sendBuffer = createTestArrayBuffer(8);
  let sendBufferUint8View = new Uint8Array(sendBuffer);
  let bufferLength = sendBufferUint8View.length;
  let receiveBuffer = new ArrayBuffer(bufferLength);
  let receiveBufferUint8View = new Uint8Array(receiveBuffer);
  let bytesToReceive = bufferLength;
  let hasReadError = false;
  let expectDisconnect = false;

  let operation = 0;
  const doNextOperation = function() {
    switch (operation++) {
      case 0:
        serial.getDevices(onGetDevices);
        break;
      case 1:
        serial.getConnections(onGetConnectionsEmpty);
        break;
      case 2: {
        const bitrate = 57600;
        console.log(
            `Connecting to serial device ${serialPort} at ${bitrate} bps.`);
        serial.connect(serialPort, {bitrate: bitrate}, onConnect);
        break;
      }
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
      case 11: {
        const bitrate = 115200;
        console.log(
            `Reconnecting to serial device ${serialPort} at ${bitrate} bps.`);
        serial.connect(serialPort, {bitrate: bitrate}, onConnect);
        break;
      }
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

  const skipToTearDown = function() {
    operation = 50;
    doNextOperation();
  };

  const repeatOperation = function() {
    operation--;
    doNextOperation();
  };

  const onGetConnectionsEmpty = function(connectionInfos) {
    chrome.test.assertEq(0, connectionInfos.length);
    doNextOperation();
  };

  const onGetConnectionsOne = function(connectionInfos) {
    chrome.test.assertEq(1, connectionInfos.length);
    chrome.test.assertEq(connectionId, connectionInfos[0].connectionId);
    doNextOperation();
  };

  const onDisconnect = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  const onReceive = function(receiveInfo) {
    const data = new Uint8Array(receiveInfo.data);
    bytesToReceive -= data.length;
    const receiveBufferIndex = bufferLength - bytesToReceive - data.length;
    for (let i = 0; i < data.length; i++) {
      receiveBufferUint8View[i + receiveBufferIndex] = data[i];
    }
    if (bytesToReceive == 0) {
      chrome.test.assertEq(
          sendBufferUint8View, receiveBufferUint8View,
          'Buffer received was not equal to buffer sent.');
      if (!hasReadError) {
        doNextOperation();
      }
    } else if (--receiveTries <= 0) {
      chrome.test.fail('receive() failed to return requested number of bytes.');
    }
  };

  const onReceiveError = function(errorInfo) {
    chrome.test.assertEq(
        connectionId, errorInfo.connectionId,
        'Unmatch connectionId for ReceiveError');
    if (expectDisconnect && errorInfo.error === 'disconnected') {
      expectDisconnect = false;
      doNextOperation();
      return;
    }
    hasReadError = true;
    if (errorInfo.error == 'parity_error') {
      serial.getInfo(connectionId, onGetInfoToReconnect);
    } else {
      chrome.test.fail(`Failed to receive serial data: ${errorInfo.error}`);
    }
  };

  const onGetInfoToReconnect = function(connectionInfo) {
    chrome.test.assertEq(
        true, connectionInfo.paused,
        'Failed to pause connection on read error.');
    // Try to reconnect the read data pipe.
    serial.setPaused(connectionId, false, () => {
      serial.getInfo(connectionId, onGetInfoAfterReconnect);
    });
  };

  const onGetInfoAfterReconnect = function(connectionInfo) {
    if (connectionInfo.paused != false) {
      chrome.test.fail('Failed to reconnect on read error.');
    }
    hasReadError = false;
    doNextOperation();
  };

  const onSend = function(sendInfo) {
    chrome.test.assertEq(
        bufferLength, sendInfo.bytesSent, 'Failed to send byte.');
  };

  const onGetControlSignals = function(options) {
    chrome.test.assertNe('undefined', typeof options.dcd, 'No DCD set');
    chrome.test.assertNe('undefined', typeof options.cts, 'No CTS set');
    chrome.test.assertNe('undefined', typeof options.ri, 'No RI set');
    chrome.test.assertNe('undefined', typeof options.dsr, 'No DSR set');
    doNextOperation();
  };

  const onSetControlSignals = function(result) {
    chrome.test.assertTrue(result);
    doNextOperation();
  };

  const onConnect = function(connectionInfo) {
    chrome.test.assertTrue(
        !!connectionInfo, 'Failed to connect to serial port.');
    connectionId = connectionInfo.connectionId;
    doNextOperation();
  };

  const onGetDevices = function(devices) {
    if (devices.length > 0) {
      let portNumber = 0;
      while (portNumber < devices.length) {
        if (shouldSkipPort(devices[portNumber].path)) {
          portNumber++;
          continue;
        } else {
          break;
        }
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
