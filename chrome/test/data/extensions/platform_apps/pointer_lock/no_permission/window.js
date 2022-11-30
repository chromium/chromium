// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function () {
  chrome.test.sendMessage('Launched', function() {
    chrome.test.runTests([
      function requestPointerLock() {
        document.onpointerlockchange = chrome.test.fale;
        document.onpointerlockerror = chrome.test.succeed;
        document.body.requestPointerLock();
      },
    ]);
  });
}
