// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TCP tests use an HTTP server configured to echo back the request body
// as the response body.
const tcpRequest = "POST /echo HTTP/1.1\r\n" +
    "Content-Length: 19\r\n\r\n" +
    "0100000005320000005";
    const tcpExpectedResponsePattern = /\n0100000005320000005$/;

// UDP tests use a server that just echoes back the request.
const udpRequest = "0100000005320000005";
const udpExpectedResponsePattern = /^0100000005320000005$/;

const socket = chrome.socket;
var address;
var bytesWritten = 0;
var dataAsString;
var dataRead = [];
var port = -1;
var protocol = "none";
var socketId = 0;
var succeeded = false;
var waitCount = 0;
var request = "<this should be set based on protocol>";
var expectedResponsePattern = "<this, too>";

// Many thanks to Dennis for his StackOverflow answer: http://goo.gl/UDanx
// Since amended to handle BlobBuilder deprecation.
function string2ArrayBuffer(string, callback) {
  var blob = new Blob([string]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsArrayBuffer(blob);
}

function arrayBuffer2String(buf, callback) {
  var blob = new Blob([new Uint8Array(buf)]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsText(blob);
}

function assertDataMatch(expecterDataPattern, data) {
  var match = !!data.match(expecterDataPattern);
  chrome.test.assertTrue(match, "Received data does not match. " +
    "Expected pattern: \"" + expecterDataPattern + "\" - " +
    "Data received: \"" + data + "\".");
}

var testSocketCreation = function() {
  function onCreate(socketInfo) {
    function onGetInfo(info) {
      chrome.test.assertEq(info.socketType, protocol);
      chrome.test.assertFalse(info.connected);

      if (info.peerAddress || info.peerPort) {
        chrome.test.fail('Unconnected socket should not have peer');
      }
      if (info.localAddress || info.localPort) {
        chrome.test.fail('Unconnected socket should not have local binding');
      }

      socket.destroy(socketInfo.socketId);
      socket.getInfo(socketInfo.socketId, function(info) {
        chrome.test.assertEq(undefined, info);
        chrome.test.succeed();
      });
    }

    chrome.test.assertTrue(socketInfo.socketId > 0);

    // Obtaining socket information before a connect() call should be safe, but
    // return empty values.
    socket.getInfo(socketInfo.socketId, onGetInfo);
  }

  socket.create(protocol, {}, onCreate);
};


var testGetInfo = function() {
};

function onDataRead(readInfo) {
  if (readInfo.resultCode > 0 || readInfo.data.byteLength > 0) {
    chrome.test.assertEq(readInfo.resultCode, readInfo.data.byteLength);
  }

  arrayBuffer2String(readInfo.data, function(s) {
    dataAsString = s;  // save this for error reporting
    assertDataMatch(expectedResponsePattern, dataAsString);
    succeeded = true;
    chrome.test.succeed();
  });
}

function onWriteOrSendToComplete(writeInfo) {
  bytesWritten += writeInfo.bytesWritten;
  if (bytesWritten == request.length) {
    if (protocol == "tcp")
      socket.read(socketId, onDataRead);
    else
      socket.recvFrom(socketId, onDataRead);
  }
}

function onSetKeepAlive(result) {
  if (protocol == "tcp")
    chrome.test.assertTrue(result, "setKeepAlive failed for TCP.");
  else
    chrome.test.assertFalse(result, "setKeepAlive did not fail for UDP.");

  string2ArrayBuffer(request, function(arrayBuffer) {
      if (protocol == "tcp")
        socket.write(socketId, arrayBuffer, onWriteOrSendToComplete);
      else
        socket.sendTo(socketId, arrayBuffer, address, port,
                      onWriteOrSendToComplete);
    });
}

function onSetNoDelay(result) {
  if (protocol == "tcp")
    chrome.test.assertTrue(result, "setNoDelay failed for TCP.");
  else
    chrome.test.assertFalse(result, "setNoDelay did not fail for UDP.");
  socket.setKeepAlive(socketId, true, 1000, onSetKeepAlive);
}

function onGetInfo(result) {
  chrome.test.assertTrue(!!result.localAddress,
                         "Bound socket should always have local address");
  chrome.test.assertTrue(!!result.localPort,
                         "Bound socket should always have local port");
  chrome.test.assertEq(result.socketType, protocol, "Unexpected socketType");

  if (protocol == "tcp") {
    // NOTE: We're always called with 'localhost', but getInfo will only return
    // IPs, not names.
    chrome.test.assertEq(result.peerAddress, "127.0.0.1",
                         "Peer addresss should be the listen server");
    chrome.test.assertEq(result.peerPort, port,
                         "Peer port should be the listen server");
    chrome.test.assertTrue(result.connected, "Socket should be connected");
  } else {
    chrome.test.assertFalse(result.connected, "UDP socket was not connected");
    chrome.test.assertTrue(!result.peerAddress,
        "Unconnected UDP socket should not have peer address");
    chrome.test.assertTrue(!result.peerPort,
        "Unconnected UDP socket should not have peer port");
  }

  socket.setNoDelay(socketId, true, onSetNoDelay);
}

function onConnectOrBindComplete(result) {
  chrome.test.assertEq(0, result,
                       "Connect or bind failed with error " + result);
  if (result == 0) {
    socket.getInfo(socketId, onGetInfo);
  }
}

function onCreate(socketInfo) {
  socketId = socketInfo.socketId;
  chrome.test.assertTrue(socketId > 0, "failed to create socket");
  if (protocol == "tcp")
    socket.connect(socketId, address, port, onConnectOrBindComplete);
  else
    socket.bind(socketId, "0.0.0.0", 0, onConnectOrBindComplete);
}

function waitForBlockingOperation() {
  if (++waitCount < 10) {
    setTimeout(waitForBlockingOperation, 1000);
  } else {
    // We weren't able to succeed in the given time.
    chrome.test.fail("Operations didn't complete after " + waitCount + " " +
                     "seconds. Response so far was <" + dataAsString + ">.");
  }
}

var testSending = function() {
  dataRead = "";
  succeeded = false;
  waitCount = 0;

  setTimeout(waitForBlockingOperation, 1000);
  socket.create(protocol, {}, onCreate);
};

// Tests listening on a socket and sending/receiving from accepted sockets.
var testSocketListening = function() {
  var tmpSocketId = 0;

  function onServerSocketAccept(acceptInfo) {
    chrome.test.assertEq(0, acceptInfo.resultCode);
    var acceptedSocketId = acceptInfo.socketId;
    socket.read(acceptedSocketId, function(readInfo) {
      arrayBuffer2String(readInfo.data, function (s) {
        assertDataMatch(request, s);
        // Rather than using a timeout, use another read to detect the peer
        // termination.
        socket.read(acceptedSocketId, function(readInfo2) {
          chrome.test.assertEq(0, readInfo2.resultCode);
          socket.getInfo(acceptedSocketId, function(info) {
            chrome.test.assertFalse(info.connected);
            // Use a third read to make sure net::ERR_SOCKET_NOT_CONNECTED (-15)
            // is received for subsequent reads.
            socket.read(acceptedSocketId, function(readInfo3) {
              chrome.test.assertEq(-15, readInfo3.resultCode);
              socket.destroy(socketId);
              chrome.test.succeed();
            });
          });
        });
      });
    });
  }

  function onListen(result) {
    chrome.test.assertEq(0, result, "Listen failed.");
    socket.accept(socketId, onServerSocketAccept);

    // Trying to schedule a second accept callback should fail.
    socket.accept(socketId, function(acceptInfo) {
      chrome.test.assertEq(-2, acceptInfo.resultCode);
    });

    // Create a new socket to connect to the TCP server.
    socket.create('tcp', {}, function(socketInfo) {
      tmpSocketId = socketInfo.socketId;
      socket.connect(tmpSocketId, address, port,
        function(result) {
          chrome.test.assertEq(0, result, "Connect failed");

          // Write.
          string2ArrayBuffer(request, function(buf) {
            socket.write(tmpSocketId, buf, function() {
              socket.disconnect(tmpSocketId);
            });
          });
        });
    });
  }

  function onServerSocketCreate(socketInfo) {
    socketId = socketInfo.socketId;
    socket.listen(socketId, address, port, onListen);
  }

  socket.create('tcp', {}, onServerSocketCreate);
};

// Tests creation of a TCP listening socket on a port that is already in use.
var testSocketListenInUse = function() {
  var tmpSocketId;

  function onAccept(result) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(-2, result.resultCode);
    socket.destroy(socketId);
    socket.destroy(tmpSocketId);
    chrome.test.succeed();
  }

  function onSecondSocketListen(result) {
    chrome.test.assertLastError("Could not listen on the specified port.");
    chrome.test.assertEq(-147, result);
    // Calling accept on this socket should fail since it isn't listening.
    socket.accept(tmpSocketId, onAccept);
  }

  function onSecondSocketCreate(socketInfo) {
    chrome.test.assertNoLastError();
    tmpSocketId = socketInfo.socketId;
    socket.listen(tmpSocketId, address, port, onSecondSocketListen);
  }

  function onFirstSocketListen(result) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(0, result);
    socket.create('tcp', {}, onSecondSocketCreate);
  }

  function onFirstSocketCreate(socketInfo) {
    chrome.test.assertNoLastError();
    socketId = socketInfo.socketId;
    socket.listen(socketId, address, port, onFirstSocketListen);
  }

  socket.create('tcp', {}, onFirstSocketCreate);
};

var testPendingCallback = function() {
  dataRead = "";
  succeeded = false;
  waitCount = 0;

  console.log("calling create");
  chrome.socket.create(protocol, null, onCreate);

  function onCreate(createInfo) {
    chrome.test.assertTrue(createInfo.socketId > 0, "failed to create socket");
    socketId = createInfo.socketId;
    console.log("calling connect");
    if (protocol == "tcp")
      chrome.socket.connect(socketId, address, port, onConnect1);
    else
      chrome.socket.bind(socketId, "0.0.0.0", 0, onConnect1);
  }

  function onConnect1(result) {
    chrome.test.assertEq(0, result, "failed to connect");
    console.log("Socket connect: result=" + result, chrome.runtime.lastError);

    console.log("calling read with readCB1 callback");
    if (protocol == "tcp")
      chrome.socket.read(socketId, readCB1);
    else
      chrome.socket.recvFrom(socketId, readCB1);

    console.log("calling disconnect");
    chrome.socket.disconnect(socketId);

    console.log("calling connect");
    if (protocol == "tcp")
      chrome.socket.connect(socketId, address, port, onConnect2);
    else
      chrome.socket.bind(socketId, "0.0.0.0", 0, onConnect2);
  }

  function onConnect2(result) {
    chrome.test.assertEq(0, result, "failed to connect");
    console.log("Socket connect: result=" + result, chrome.runtime.lastError);

    console.log("calling read with readCB2 callback");
    if (protocol == "tcp")
      chrome.socket.read(socketId, readCB2);
    else
      chrome.socket.recvFrom(socketId, readCB2);

    string2ArrayBuffer(request, function (arrayBuffer) {
      if (protocol == "tcp")
        chrome.socket.write(socketId, arrayBuffer, onWriteComplete);
      else
        chrome.socket.sendTo(
            socketId, arrayBuffer, address, port, onWriteComplete);
    });
  }

  function onWriteComplete(res) {
    console.log("write callback: bytesWritten=" + res.bytesWritten);
  }

  // Callback 1 for initial read call
  function readCB1(readInfo) {
    console.log("Socket read CB1: result=" + readInfo.resultCode,
        chrome.runtime.lastError);
    // We disconnect the socket right after calling read(), so behavior here
    // is undefined.
    // TODO(devlin): Why do we do that? What are we trying to do?
  }

  // Second callback, for read call after re-connect
  function readCB2(readInfo) {
    console.log("Socket read CB2: result=" + readInfo.resultCode,
        chrome.runtime.lastError);
    if (readInfo.resultCode === -1) {
      chrome.test.fail("Unable to register a read 2nd callback on the socket!");
    } else if (readInfo.resultCode < 0) {
      chrome.test.fail("Error reading from socket: " + readInfo.resultCode);
    }
    else {
      arrayBuffer2String(readInfo.data, function (s) {
        assertDataMatch(expectedResponsePattern, s);
        console.log("Success!");
        succeeded = true;
        chrome.test.succeed();
      });
    }
  }
}

// See http://crbug.com/418229.
var testUsingTCPSocketOnUDPMethods = function() {
  if (protocol == "udp") {
    socket.create("tcp", function(createInfo) {
      socket.recvFrom(createInfo.socketId, 256, function(recvFromInfo) {
        chrome.test.assertTrue(recvFromInfo.resultCode < 0);
        chrome.test.succeed();
      });
    });

    function onSendToComplete(writeInfo) {
      chrome.test.assertTrue(writeInfo.bytesWritten < 0);
      chrome.test.succeed();
    }

    string2ArrayBuffer(request, function(arrayBuffer) {
      socket.create("tcp", function(createInfo) {
          socket.sendTo(createInfo.socketId, arrayBuffer, address, port,
                        onSendToComplete);
      });
    });
  } else {
    // We only run this test when the protocol is UDP to
    // avoid running it multiple times unnecessarily.
    chrome.test.succeed();
  }
};

var testWriteQuota = function() {
  console.log("calling create, protoocol=", protocol);
  chrome.socket.create(protocol, {}, onCreate);

  function onCreate(createInfo) {
    chrome.test.assertTrue(createInfo.socketId > 0, "failed to create socket");
    socketId = createInfo.socketId;
    if (protocol == "tcp") {
      console.log("calling connect");
      chrome.socket.connect(socketId, address, port, onConnectOrBindComplete);
    } else {
      console.log("calling bind");
      socket.bind(socketId, "0.0.0.0", 0, onConnectOrBindComplete);
    }
  }

  function onConnectOrBindComplete(result) {
    chrome.test.assertEq(0, result, "failed to connect");
    console.log("Socket connect: result=" + result, chrome.runtime.lastError);

    string2ArrayBuffer(request, function (arrayBuffer) {
      if (protocol == "tcp") {
        chrome.socket.write(socketId, arrayBuffer, onComplete);
      } else {
        socket.sendTo(socketId, arrayBuffer, address, port,
                      onComplete);
      }
    });
  }

  function onComplete(result) {
    console.log("onComplete: result=" + result, chrome.runtime.lastError);
    if (chrome.runtime.lastError &&
        chrome.runtime.lastError.message == "Exceeded write quota.") {
      chrome.test.succeed();
      return;
    }

    chrome.test.fail('Write quota not enforced');
  }
}

var onMessageReply = function(message) {
  var parts = message.split(":");
  var test_type = parts[0];
  address = parts[1];
  port = parseInt(parts[2]);
  console.log("Running tests, protocol " + test_type + ", echo server " +
              address + ":" + port);
  if (test_type == 'tcp_server') {
    chrome.test.runTests([
        testSocketListening,
        testSocketListenInUse
    ]);
  } else if (test_type == 'multicast') {
    console.log("Running multicast tests");
    chrome.test.runTests([ testMulticast ]);
  } else if (test_type == 'tcp_write_quota') {
    console.log("Running TCP write quota tests");
    protocol = "tcp";
    chrome.test.runTests([ testWriteQuota ]);
  } else if (test_type == 'udp_sendTo_quota') {
    console.log("Running UDP sendTo write quota tests");
    protocol = "udp";
    chrome.test.runTests([ testWriteQuota ]);
  } else {
    protocol = test_type;
    if (protocol == "udp") {
      request = udpRequest;
      expectedResponsePattern = udpExpectedResponsePattern;
    } else {
      request = tcpRequest;
      expectedResponsePattern = tcpExpectedResponsePattern;
    }
    chrome.test.runTests([
        testSocketCreation,
        testSending,
        testPendingCallback,
        testUsingTCPSocketOnUDPMethods]);
  }
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
