// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.memory api test
// browser_tests --gtest_filter="SystemMemoryApiMV3Test.*"
// run_android_browsertests -f "SystemMemoryApiMV3Test*"

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.system.memory.getInfo(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4096, result.capacity);
        chrome.test.assertEq(1024, result.availableCapacity);
      }));
    }
  }
]);
