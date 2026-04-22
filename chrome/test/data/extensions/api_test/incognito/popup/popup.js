// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

chrome.test.runTests([
  function getCurrentWindow() {
    // With incognito enabled, we should get our current window (which should
    // be incognito).
    chrome.windows.getCurrent(pass(function(win) {
      assertTrue(win.incognito);
    }));
  },
]);
