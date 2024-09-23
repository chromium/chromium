// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40697472) automate packing procedure
// Must be packed to ../enterprise_networking_attributes.crx using the private
// key ../enterprise_networking_attributes.pem .

let expectedErrorMessage, expectedResult;

const availableTests = [
  function failure() {
    chrome.enterprise.networkingAttributes.getNetworkDetails((details) => {
      chrome.test.assertLastError(expectedErrorMessage);
      chrome.test.succeed();
    });
  },
  function success() {
    chrome.enterprise.networkingAttributes.getNetworkDetails((details) => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expectedResult, details);
      chrome.test.succeed();
    });
  },
];

chrome.test.getConfig(function(config) {
  const args = JSON.parse(config.customArg);
  expectedResult = args.expectedResult;
  expectedErrorMessage = args.expectedErrorMessage;
  const testName = args.testName;

  const tests = availableTests.filter((testFunc) => {
    return testFunc.name === testName;
  });
  if (tests.length !== 1) {
    chrome.test.notifyFail('Test not found ' + testName);
    return;
  }

  chrome.test.runTests(tests);
});
