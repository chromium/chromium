// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const socket = chrome.socket;

var onListen = function(result) {
  chrome.test.assertEq(0, result);
  chrome.test.succeed();
};

var onCreate = function(socketInfo) {
  sid = socketInfo.socketId;
  socket.listen(sid, '0.0.0.0', 1234, onListen);
};

chrome.test.runTests([
  function bind() {
    socket.create('tcp', {}, onCreate);
  }
]);
