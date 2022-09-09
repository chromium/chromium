// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systemPrivate.getUpdateStatus test for Chrome
// browser_tests --gtest_filter="GetUpdateStatusApiTest.Progress"

chrome.test.runTests([
  function notAvailable() {
    chrome.systemPrivate.getUpdateStatus(function(status) {
      chrome.test.assertEq(status.state, "NotAvailable");
      chrome.test.assertEq(status.downloadProgress, 0.0);
      chrome.test.succeed();
    });
  },
  function updating() {
    chrome.systemPrivate.getUpdateStatus(function(status) {
      chrome.test.assertEq(status.state, "Updating");
      chrome.test.assertEq(status.downloadProgress, 0.5);
      chrome.test.succeed();
    });
  },
  function needRestart() {
    chrome.systemPrivate.getUpdateStatus(function(status) {
      chrome.test.assertEq(status.state, "NeedRestart");
      chrome.test.assertEq(status.downloadProgress, 1);
      chrome.test.succeed();
    });
  }
]);
