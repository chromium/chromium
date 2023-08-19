// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  chrome.app.window.create('app_main.html',
      { 'width': 1920,
        'height': 1080 },
      function(window) {
  });
});
