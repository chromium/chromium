// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

chrome.test.runTests([
  function getCurrentWindow() {
    // With incognito enabled, we should get our current window (which should
    // be incognito).
    chrome.windows.getCurrent(pass(function(win) {
      assertTrue(win.incognito);
    }));
  }
]);
