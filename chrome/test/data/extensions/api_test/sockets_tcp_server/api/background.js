// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// net/tools/testserver/testserver.py is picky about the format of what it
// calls its "echo" messages. One might go so far as to mutter to oneself that
// it isn't an echo server at all.
//
// The response is based on the request but obfuscated using a random key.
const request = '0100000005320000005hello';

let address;
let port = -1;
let socketId = 0;
let succeeded = false;

function string2ArrayBuffer(string, callback) {
  const blob = new Blob([string]);
  const f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsArrayBuffer(blob);
}

function arrayBuffer2String(buf, callback) {
  const blob = new Blob([new Uint8Array(buf)]);
  const f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsText(blob);
}

// Tests listening on a socket and sending/receiving from accepted sockets.
const testSocketListening = function() {
  let tmpSocketId = 0;

  chrome.sockets.tcpServer.create({}, onServerSocketCreate);

  function onServerSocketCreate(socketInfo) {
    console.info('Server socket created: sd=' + socketInfo.socketId);
    socketId = socketInfo.socketId;
    chrome.sockets.tcpServer.getInfo(socketId, onGetInfo);
  }

  function onGetInfo(socketInfo) {
    chrome.test.assertEq(socketInfo.socketId, socketId);
    chrome.sockets.tcpServer.listen(socketId, address, port, onListen);
  }

  function onListen(result) {
    console.info(
        'Server socket \'listen\' completed: sd=' + socketId +
        ', result=' + result);
    chrome.test.assertEq(0, result, 'Listen failed.');
    chrome.sockets.tcpServer.onAccept.addListener(onServerSocketAccept);
    chrome.sockets.tcp.onReceive.addListener(onReceive);
    chrome.sockets.tcp.onReceiveError.addListener(onReceiveError);

    // Create a new socket to connect to the TCP server.
    chrome.sockets.tcp.create({}, function(socketInfo) {
      console.info('Client socket created: sd=' + socketInfo.socketId);
      tmpSocketId = socketInfo.socketId;
      chrome.sockets.tcp.connect(tmpSocketId, address, port, function(result) {
        console.info('Client socket connected: sd=' + tmpSocketId);
        chrome.test.assertEq(0, result, 'Connect failed');

        // Write.
        string2ArrayBuffer(request, function(buf) {
          chrome.sockets.tcp.send(tmpSocketId, buf, function(sendInfo) {
            console.info(
                'Client socket data sent: sd=' + tmpSocketId +
                ', result=' + sendInfo.resultCode);
            chrome.sockets.tcp.disconnect(tmpSocketId, function() {
              console.info('Client socket disconnected: sd=' + tmpSocketId);
            });
          });
        });
      });
    });
  }

  let clientSocketId;

  function onServerSocketAccept(acceptInfo) {
    console.info(
        'Server socket \'accept\' event: sd=' + acceptInfo.socketId +
        ', client sd=' + acceptInfo.clientSocketId);
    chrome.test.assertEq(socketId, acceptInfo.socketId, 'Wrong server socket.');
    chrome.test.assertTrue(acceptInfo.clientSocketId > 0);
    clientSocketId = acceptInfo.clientSocketId;
    chrome.sockets.tcp.setPaused(clientSocketId, false, function() {});
  }

  function onReceive(receiveInfo) {
    console.info(
        'Client socket \'receive\' event: sd=' + receiveInfo.socketId +
        ', bytes=' + receiveInfo.data.byteLength);
    chrome.test.assertEq(
        clientSocketId, receiveInfo.socketId, 'Received data on wrong socket');
    if (receiveInfo.data.byteLength === 0) {
      return;
    }
    arrayBuffer2String(receiveInfo.data, function(s) {
      const match = !!s.match(request);
      chrome.test.assertTrue(match, 'Received data does not match.');
      succeeded = true;
      // Test whether socket.getInfo correctly reflects the connection status
      // if the peer has closed the connection.
      setTimeout(function() {
        chrome.sockets.tcp.getInfo(receiveInfo.socketId, function(info) {
          chrome.test.assertFalse(info.connected);
          chrome.test.succeed();
        });
      }, 500);
    });
  }

  function onReceiveError(receiveInfo) {
    console.info(
        'Client socket \'receive error\' event: sd=' + receiveInfo.socketId +
        ', result=' + receiveInfo.resultCode);
    // chrome.test.fail("Receive failed.");
  }
};

const onMessageReply = function(message) {
  const parts = message.split(':');
  const test_type = parts[0];
  address = parts[1];
  port = parseInt(parts[2]);
  console.info(
      'Running tests, protocol ' + test_type + ', echo server ' + address +
      ':' + port);
  if (test_type === 'tcp_server') {
    chrome.test.runTests([testSocketListening]);
  }
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage('info_please', onMessageReply);
