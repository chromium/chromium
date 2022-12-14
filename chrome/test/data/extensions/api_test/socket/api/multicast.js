// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for multicast UDP socket.
function testMulticast() {
  function randomHexString(count) {
    var result = '';
    for (var i = 0; i < count; i++) {
      result += (Math.random() * 16 >> 0).toString(16);
    }
    return result;
  }

  var kMulticastAddress = "237.132.100.133";
  var kTestMessageLength = 128;
  var kTestMessage = randomHexString(128);
  var kPort = 11103;

  function arrayBufferToString(arrayBuffer) {
    // UTF-16LE
    return String.fromCharCode.apply(String, new Uint16Array(arrayBuffer));
  }

  function stringToArrayBuffer(string) {
    // UTF-16LE
    var buf = new ArrayBuffer(string.length * 2);
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = string.length; i < strLen; i++) {
      bufView[i] = string.charCodeAt(i);
    }
    return buf;
  }

  function waitForMessage(socketId, callback) {
    var cancelled = false;
    var relayCanceller = null;
    socket.recvFrom(socketId, 1024, function (result) {
      if (cancelled)
        return;

      if (result.resultCode == kTestMessageLength * 2 &&
          kTestMessage === arrayBufferToString(result.data)) {
        callback(cancelled);
      } else {
        // Restart waiting.
        relayCanceller = waitForMessage(socketId, callback);
      }
    });
    return function canceller() {
      if (relayCanceller) {
        relayCanceller();
      } else {
        cancelled = true;
        callback(true);
      }
    };
  }

  function testMulticastSettings() {
    socket.create('udp', {}, function (socketInfo) {
      var socket_id;
      if (socketInfo) {
        socket_id = socketInfo.socketId;
        socket.setMulticastTimeToLive(socket_id, 0, function (result) {
          chrome.test.assertEq(0, result,
              "Error setting multicast time to live.");
          socket.setMulticastTimeToLive(socket_id, -3, function (result) {
            chrome.test.assertEq(-4, result,
                "Error setting multicast time to live.");
            socket.setMulticastLoopbackMode(socket_id, false,
                function (result) {
              chrome.test.assertEq(0, result,
                  "Error setting multicast loop back mode.");
              socket.setMulticastLoopbackMode(socket_id, true,
                  function (result) {
                chrome.test.assertEq(0, result,
                    "Error setting multicast loop back mode.");
                socket.destroy(socket_id);
                testMulticastRecv();
              });
            });
          });
        });
      } else {
        chrome.test.fail("Cannot create server udp socket");
      }
    });
  }

  function testSendMessage(message, address) {
    // Send the UDP message to the address with multicast ttl = 0.
    address = address || kMulticastAddress;
    socket.create('udp', {}, function (socketInfo) {
      var clientSocketId;
      if (socketInfo) {
        clientSocketId = socketInfo.socketId;
        chrome.test.assertTrue(clientSocketId > 0,
            "Cannot create client udp socket.");
        socket.setMulticastTimeToLive(clientSocketId, 0, function (result) {
          chrome.test.assertEq(0, result,
              "Cannot create client udp socket.");
          socket.connect(clientSocketId, address, kPort, function (result) {
            chrome.test.assertEq(0, result,
                "Cannot connect to localhost.");
            socket.write(clientSocketId, stringToArrayBuffer(kTestMessage),
                function (result) {
                  chrome.test.assertTrue(result.bytesWritten >= 0,
                      "Send to failed. " + JSON.stringify(result));
                  socket.destroy(clientSocketId);
                });
          });
        });
      } else {
        chrome.test.fail("Cannot create client udp socket");
      }
    });
  }

  function recvBeforeAddMembership(serverSocketId) {
    var recvTimeout;
    var canceller = waitForMessage(serverSocketId, function (cancelled) {
      clearTimeout(recvTimeout);
      if (cancelled) {
        recvWithMembership(serverSocketId);
      } else {
        chrome.test.fail("Received message before joining the group");
      }
    });
    testSendMessage(kTestMessage); // Meant to be lost.
    recvTimeout = setTimeout(function () {
      canceller();
      testSendMessage(kTestMessage, "127.0.0.1", kPort);
    }, 2000);
  }

  function recvWithMembership(serverSocketId) {
    // Join group.
    socket.joinGroup(serverSocketId, kMulticastAddress, function (result) {
      chrome.test.assertEq(0, result, "Join group failed.");
      var recvTimeout;
      var canceller = waitForMessage(serverSocketId, function (cancelled) {
        clearTimeout(recvTimeout);
        if (!cancelled) {
          recvWithoutMembership(serverSocketId);
        } else {
          chrome.test.fail("Faild to receive message after joining the group");
        }
      });
      testSendMessage(kTestMessage);
      recvTimeout = setTimeout(function () {
        canceller();
        socket.destroy(serverSocketId);
        chrome.test.fail("Cannot receive from multicast group.");
      }, 2000);
    });
  }

  function recvWithoutMembership(serverSocketId) {
    socket.leaveGroup(serverSocketId, kMulticastAddress, function (result) {
      chrome.test.assertEq(0, result, "leave group failed.");
      var recvTimeout;
      var canceller = waitForMessage(serverSocketId, function (cancelled) {
        clearTimeout(recvTimeout);
        if (cancelled) {
          leaveGroupAndDisconnect(serverSocketId);
        } else {
          chrome.test.fail("Received message after leaving the group");
          socket.destroy(serverSocketId);
        }
      });
      testSendMessage(request);
      recvTimeout = setTimeout(function () {
        canceller();
      }, 2000);
    });
  }

  function leaveGroupAndDisconnect(serverSocketId) {
    socket.joinGroup(serverSocketId, kMulticastAddress, function (result) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(0, result, "Join group failed.");
      socket.leaveGroup(serverSocketId, kMulticastAddress, () => {
        chrome.test.assertEq(0, result, "Leave group failed.");
        socket.destroy(serverSocketId);
        chrome.test.succeed();
      });
      socket.disconnect(serverSocketId);
    });
  }

  function testMulticastRecv() {
    socket.create('udp', {}, function (socketInfo) {
      var serverSocketId;
      if (socketInfo) {
        serverSocketId = socketInfo.socketId;
        socket.bind(serverSocketId, "0.0.0.0", kPort, function (result) {
          chrome.test.assertEq(0, result, "Bind failed.");
          recvBeforeAddMembership(serverSocketId);
        });
      } else {
        chrome.test.fail("Cannot create server udp socket");
      }
    });
  }

  testMulticastSettings();
}
