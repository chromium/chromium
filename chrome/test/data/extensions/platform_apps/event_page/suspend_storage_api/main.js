// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {});
});

chrome.runtime.onSuspend.addListener(function() {
  var now = new Date();
  chrome.storage.local.set({"last_save": now.toLocaleString()}, function() {
    console.log("Finished writing last_save: " + now.toLocaleString());
  });
});
