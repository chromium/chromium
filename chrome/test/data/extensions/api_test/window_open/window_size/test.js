// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openSizedWindow() {
    chrome.windows.create(
      // Note: width and height must be larger than the minimum window size
      // and smaller than the max (screen) size.
      { 'url': chrome.runtime.getURL('popup.html'), 'type': 'popup',
        'width': 200, 'height': 200 },
      chrome.test.callbackPass(function(win) {
        chrome.test.assertEq(200, win.width);
        chrome.test.assertEq(200, win.height);
      }));
  }
]);
