// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var createCount = 0;

var launchWindow = function() {
  chrome.app.window.create('main.html', {}, function (win) {
    if (createCount < 3) {
      ++createCount;
      chrome.test.log('Attempt no #' + createCount);
      win.close();
      launchWindow();
    } else {
      // Due to https://crbug.com/620194, we cannot sendMessage() synchronously
      // from this callback. Let |win.onload| run before sendMessage is run.
      setTimeout(function() {
        chrome.test.sendMessage('done-close-on-loadcommit');
      }, 0);
    }
  });
};

chrome.app.runtime.onLaunched.addListener(function() {
  launchWindow();
});
