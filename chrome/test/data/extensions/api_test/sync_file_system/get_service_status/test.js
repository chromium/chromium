// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testSteps = [
  function () {
    chrome.syncFileSystem.requestFileSystem(
        chrome.test.callbackPass(testSteps.shift()));
  },
  function () {
    chrome.syncFileSystem.getServiceStatus(
        chrome.test.callbackPass(testSteps.shift()));
  },
  function (status) {
    chrome.test.getConfig(function(config) {
      featureEnabled = config.customArg == "enabled";
      expectedStatus = featureEnabled ? 'authentication_required' : 'disabled';
      chrome.test.assertEq(expectedStatus, status);
    })
  }
];

chrome.test.runTests([
  testSteps.shift()
]);
