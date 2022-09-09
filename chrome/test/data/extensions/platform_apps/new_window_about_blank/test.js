// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('about:blank', {}, function (createdWindow) {
    if (createdWindow) {
      chrome.test.notifyFail("chrome.app.window.create() with about:blank " +
                             "succeeded. That's not allowed.");
    } else {
      chrome.test.notifyPass();
    }
  })
});
