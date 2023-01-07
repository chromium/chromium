// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function (response) {
    var frameType = response == 'color' ? { color: '#ff0000' } : response;
    chrome.app.window.create('main.html', {
      frame: frameType,
      outerBounds: {
        left: 10,
        top: 11,
        width: 300,
        height: 301,
        minWidth: 200,
        minHeight: 201,
        maxWidth: 400,
        maxHeight: 401
      }
    }, function () {
      // Send this again as the test is waiting for the window to be ready.
      chrome.test.sendMessage('Launched');
    });
  });
});
