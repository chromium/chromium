// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function test() {
  chrome.permissions.request({"origins": ["file:///*"]},
                             callbackPass(function(granted) {
    chrome.test.assertTrue(granted);
    chrome.permissions.getAll(callbackPass(function(permissions) {
      chrome.test.assertEq(["file:///*"], permissions.origins);
      chrome.test.succeed();
    }));
  }));
}

chrome.test.runTests([test]);
