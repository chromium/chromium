// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnectExternal.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    if (msg.testConnectExternal) {
      port.postMessage({success: true, senderId: port.sender.id});
    }
  });
});
