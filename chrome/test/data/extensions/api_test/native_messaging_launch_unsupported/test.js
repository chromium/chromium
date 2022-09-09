// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appName = 'com.google.chrome.test.echo';

chrome.runtime.onConnectNative.addListener(port => {
  chrome.test.runTests([
    function test() {
      chrome.test.assertEq(port.sender.nativeApplication, appName);
      port.onDisconnect.addListener(chrome.test.callback(
          function() {},
          'Access to the specified native messaging host is forbidden.'));
    },
  ]);
});
