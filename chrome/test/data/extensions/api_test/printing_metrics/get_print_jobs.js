// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetPrintJobs(expectedTitle) {
  chrome.test.runTests([() => {
    chrome.printingMetrics.getPrintJobs(printJobs => {
      chrome.test.assertEq(1, printJobs.length);
      chrome.test.assertEq(expectedTitle, printJobs[0].title);
      chrome.test.succeed();
    });
  }]);
}

chrome.test.getConfig(config => {
  testGetPrintJobs(config.customArg);
});
