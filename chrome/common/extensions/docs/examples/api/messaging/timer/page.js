// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    port.postMessage({counter: msg.counter+1});
  });
});

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    sendResponse({counter: request.counter+1});
  });
