// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an example of chrome app where `kiosk_enabled` is not set in
// manifest. Chrome app without `kiosk_enabled` should not be launched in the
// kiosk session.
chrome.app.runtime.onLaunched.addListener(function (launchData) {
  chrome.app.window.create('app_main.html');
});
