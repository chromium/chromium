// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function typeNormal() {
    chrome.windows.create({'type': 'normal'}, chrome.test.callbackPass(w => {
      chrome.test.assertEq('normal', w.type);
    }));
  },
  function typePopup() {
    chrome.windows.create({'type': 'popup'}, chrome.test.callbackPass(w => {
      chrome.test.assertEq('popup', w.type);
    }));
  },
  function sizeTooBig() {
    // Setting origin + bad width/height should not crash.
    chrome.windows.create({
      'type': 'normal',
      'left': 0,
      'top': 0,
      'width': 2147483647,
      'height': 2147483647,
    }, (w => {
      chrome.test.assertLastError('Invalid value for bounds. Bounds must be ' +
                                  'at least 50% within visible screen space.');
      chrome.test.succeed();
    }));
  },
]);
