// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fail = chrome.test.callbackFail;

var GESTURE_ERROR = "This function must be called during a user gesture";

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testGesture() {
      chrome.permissions.request(
          {permissions: ['bookmarks']},
          fail(GESTURE_ERROR));
    }
  ]);
});
