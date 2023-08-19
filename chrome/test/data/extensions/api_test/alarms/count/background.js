// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create alarms that won't have time to run.
const createParams = {delayInMinutes: 60.0, periodInMinutes: 60};
const maxAlarms = 500;

chrome.test.runTests([
  function hasNoAlarms() {
    // No alarms should be initially registered, since this extension only
    // runs once.
    chrome.alarms.getAll((alarms) => {
      chrome.test.assertEq(0, alarms.length);
      chrome.test.succeed();
    });
  },

  async function setTooManyAlarms() {
    // Create the maximum allowed number of alarms.
    for (let i = 0; i < maxAlarms; ++i) {
      await new Promise((resolve) => {
        chrome.alarms.create('alarm' + i, createParams, () => {
          chrome.test.assertNoLastError();
          resolve();
        });
      });
    }
    // Try to create one more over the limit.
    chrome.alarms.create('alarm' + maxAlarms, createParams, () => {
      chrome.test.assertLastError(
          'An extension cannot have more than 500 active alarms.');
      chrome.test.succeed();
    });
  },
]);
