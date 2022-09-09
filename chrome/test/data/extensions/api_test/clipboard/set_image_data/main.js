// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var clipDataChangedCount = 0;

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  chrome.app.window.create('app_main.html', {width: 500, height: 500});
});

chrome.clipboard.onClipboardDataChanged.addListener(function() {
  clipDataChangedCount++;
  chrome.test.sendMessage('clipboard data changed ' + clipDataChangedCount);
});

