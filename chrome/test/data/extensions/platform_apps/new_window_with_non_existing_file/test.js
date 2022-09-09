// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('not_found.html', {}, function (createdWindow) {
    if (createdWindow)
      chrome.test.notifyFail();
    else
      chrome.test.notifyPass();
  })
});
