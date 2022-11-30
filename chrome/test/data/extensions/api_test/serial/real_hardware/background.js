// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testGetDevices = function() {
  var onGetDevices = function(devices) {
    chrome.test.assertTrue(devices.length == 2);
    const array = ['/dev/fakeserialmojo', '\\\\COM800\\'];
    chrome.test.assertTrue(array.indexOf(devices[0].path) >= 0);
    chrome.test.assertTrue(array.indexOf(devices[1].path) >= 0);
    chrome.test.succeed();
  }

  chrome.serial.getDevices(onGetDevices);
};

// TODO(rockot): As infrastructure is built for testing device APIs in Chrome,
// we should obviously build real hardware tests here. For now, no attempt is
// made to open real devices on the test system.

var tests = [testGetDevices];
chrome.test.runTests(tests);
