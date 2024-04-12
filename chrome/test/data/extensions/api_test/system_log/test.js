// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const testCases = [
  async function SystemLogAvailable() {
    const logOptions = { message: "Test log message" };
    await chrome.systemLog.add(logOptions);
    chrome.test.succeed();
  },
  function SystemLogUndefined() {
    chrome.test.assertTrue(chrome.systemLog === undefined);
    chrome.test.succeed();
  },
];

chrome.test.getConfig(async (config) => {
  const testName = config.customArg;
  const testCase = testCases.find((f) => f.name === testName);
  if (!testCase) {
    chrome.test.notifyFail(`Test case '${testName}' not found`);
    return;
  }

  chrome.test.runTests([testCase]);
});
