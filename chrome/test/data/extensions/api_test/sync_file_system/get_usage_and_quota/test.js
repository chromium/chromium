// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testStep = [
  function () {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  function(fs) {
    chrome.syncFileSystem.getUsageAndQuota(fs, testStep.shift());
  },
  function(info) {
    chrome.test.getConfig(function(config) {
      chrome.test.assertEq(0, info.usageBytes);

      if (config.customArg == "enabled") {
        chrome.test.assertEq(123456, info.quotaBytes);
      } else {
        chrome.test.assertNe(123456, info.quotaBytes);
      }
      chrome.test.succeed();
    });
  }
];

chrome.test.runTests([
  testStep.shift()
]);
