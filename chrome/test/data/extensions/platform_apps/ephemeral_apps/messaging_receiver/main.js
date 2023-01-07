// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {});
});

chrome.runtime.onMessageExternal.addListener(function(msg, sender, callback) {
  callback('ack_message');
});

chrome.runtime.onConnectExternal.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    port.postMessage("ack_connect_message");
  });
});
