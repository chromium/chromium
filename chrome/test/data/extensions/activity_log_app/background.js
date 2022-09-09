// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function recordDevice(device) {
  console.log("recordDevice");
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('window.html', {'width': 400,'height': 500});
  chrome.bluetooth.startDiscovery({deviceCallback: recordDevice});
  chrome.bluetooth.stopDiscovery();
});

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, response) {
  response();
});

chrome.runtime.onConnectExternal.addListener(function(port) {
  console.log("connected");
});