// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {}, function (win) {
    // Send the message PlatformAppBrowserTest.AppWindowIframe is waiting for.
    // This is the last step the test does and this step will cause the test
    // to shut down.
    chrome.test.sendMessage('APP_WINDOW_CREATE_CALLBACK');
  });
});
