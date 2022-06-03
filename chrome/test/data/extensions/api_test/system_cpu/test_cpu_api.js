// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.cpu.getInfo api test
// extensions_browsertests --gtest_filter=SystemCpuApiTest.*

chrome.test.runTests([
  function testGet() {
    var expectedProcessors = [{
      usage: {
        kernel: 1,
        user: 2,
        idle: 3,
        total: 6
      }
    }];
    for(var i = 0; i < 20; ++i) {
      chrome.system.cpu.getInfo(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(4, result.numOfProcessors);
        chrome.test.assertEq("x86", result.archName);
        chrome.test.assertEq("unknown", result.modelName);
        chrome.test.assertEq(["mmx", "avx"], result.features);
        chrome.test.assertEq(expectedProcessors, result.processors);
        chrome.test.assertEq([30.125, 40.0625], result.temperatures);
      }));
    }
  }
]);

