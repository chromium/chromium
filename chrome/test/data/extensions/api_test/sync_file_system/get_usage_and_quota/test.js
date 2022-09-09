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
    chrome.test.assertEq(0, info.usageBytes);

    // TODO(calvinlo): Update test code after default quota is made const
    // (http://crbug.com/155488).
    chrome.test.assertEq(123456, info.quotaBytes);
    chrome.test.succeed();
  }
];

chrome.test.runTests([
  testStep.shift()
]);
