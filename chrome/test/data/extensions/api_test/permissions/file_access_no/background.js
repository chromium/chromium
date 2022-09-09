// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;
var expectedError =
    "Extension must have file access enabled to request 'file:///*'.";

function test() {
  chrome.permissions.request({"origins": ["file:///*"]},
                             callbackFail(expectedError, function(granted) {
    chrome.test.assertFalse(!!granted);
    chrome.permissions.getAll(callbackPass(function(permissions) {
      chrome.test.assertEq([], permissions.origins);
      chrome.test.succeed();
    }));
  }));
}

chrome.test.runTests([test]);
