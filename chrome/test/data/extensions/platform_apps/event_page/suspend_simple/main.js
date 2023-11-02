// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {});
});

chrome.runtime.onSuspend.addListener(function() {
  var now = new Date();
  console.log("The current time is: " + now.toLocaleString());
});
