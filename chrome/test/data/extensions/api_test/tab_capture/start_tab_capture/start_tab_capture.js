// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function startTabCapture() {
    chrome.tabCapture.capture({audio: false, video: true}, function(stream) {
      if (stream)
        chrome.test.succeed();
      else
        chrome.test.fail();
    });
  }
]);
