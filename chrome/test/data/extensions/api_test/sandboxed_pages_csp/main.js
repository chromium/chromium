// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOCAL_FILE_NAME = 'local_frame.html';
var REMOTE_FILE_NAME = 'remote_frame.html';

onmessage = function(e) {
  chrome.test.assertEq(e.data, 'succeeded');
  chrome.test.succeed();
};

var loadIframeContentInSandboxedPage = function(
    localUrl, remoteUrl) {
  var sandboxedFrame = document.createElement('iframe');
  sandboxedFrame.src = 'sandboxed.html';
  sandboxedFrame.onload = function() {
    sandboxedFrame.contentWindow.postMessage(
        JSON.stringify(['load', localUrl, remoteUrl]), '*');
    sandboxedFrame.onload = null;
  };
  document.body.appendChild(sandboxedFrame);
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.runTests([
      // Local frame will succeed loading, but remote frame will fail.
      function sandboxedFrameTestLocalAndRemote() {
        var remoteUrl = 'http://localhost:' + config.testServer.port +
            '/extensions/api_test/sandboxed_pages_csp/' + REMOTE_FILE_NAME;
        loadIframeContentInSandboxedPage(
            LOCAL_FILE_NAME, remoteUrl);
      }
    ]);
  });
};
