// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let socketId;

const onListen = function(result) {
  console.info(
      'Server socket \'listen\' completed: sd=' + socketId +
      ', result=' + result);
  chrome.test.assertEq(0, result);
  chrome.test.succeed();
};

const onCreate = function(socketInfo) {
  console.info('Server socket created: sd=' + socketInfo.socketId);
  socketId = socketInfo.socketId;
  chrome.sockets.tcpServer.listen(socketId, '0.0.0.0', 1234, onListen);
};

chrome.test.runTests([
  function bind() {
    chrome.sockets.tcpServer.create({}, onCreate);
  },
]);
