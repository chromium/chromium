// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const testGetDevices = function() {
  const onGetDevices = function(devices) {
    chrome.test.assertTrue(devices.length == 0);
    chrome.test.succeed();
  };

  chrome.serial.getDevices(onGetDevices);
};

const tests = [testGetDevices];
chrome.test.runTests(tests);
