// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([

    function setUnsupportedDetectionInterval() {
      try {
        chrome.idle.setDetectionInterval(10);
        chrome.test.fail();
      } catch(e) {
        chrome.test.assertEq(
            'Error in invocation of idle.setDetectionInterval(integer ' +
            'intervalInSeconds): Error at parameter \'intervalInSeconds\': ' +
            'Value must be at least 15.',
            e.message);
        chrome.test.succeed();
      }
    },

    function setDetectionInterval() {
      chrome.idle.setDetectionInterval(Number(config.customArg));
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    },

])});
