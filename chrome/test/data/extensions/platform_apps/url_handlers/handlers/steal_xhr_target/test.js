// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  chrome.app.window.create("main.html", {}, function() {});
  chrome.test.fail("This handler shouldn't have launched");
});
