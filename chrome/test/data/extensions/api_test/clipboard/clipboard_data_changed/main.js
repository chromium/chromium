// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var clip_data_changed_count = 0;

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  console.log('onLaunched');
  chrome.app.window.create('app_main.html',
      { 'width': 500,
        'height': 500 },
      function(window) {
  });
});

chrome.clipboard.onClipboardDataChanged.addListener(function() {
  clip_data_changed_count++;
  chrome.test.sendMessage("success " + clip_data_changed_count);
});

