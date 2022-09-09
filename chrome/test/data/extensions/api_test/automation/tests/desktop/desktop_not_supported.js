// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetDesktop() {
    chrome.automation.getDesktop(function(arg) {
      assertEq(undefined, arg);
      assertEq('getDesktop is unsupported by this platform',
               chrome.runtime.lastError.message);
      chrome.test.succeed();
    });
  }
];

chrome.test.runTests(allTests);
