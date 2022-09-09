// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {

  chrome.test.sendMessage('launched', function(reply) {
    // Create window options defined in tests.
    const options = JSON.parse(reply);
    chrome.app.window.create('index.html', options, function(appWindow) {
      appWindow.focus();
    });
  });

});
