// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function hasNoAlarms() {
    // No alarms should be initially registered, since this extension only
    // runs once.
    const alarms = await chrome.alarms.getAll();
    chrome.test.assertEq(0, alarms.length);
    chrome.test.succeed();
  },

  async function setAlarms() {
    // Create alarms that won't have time to run (so they remain in storage
    // to be counted in the histogram).
    const createParams = {delayInMinutes: 120};
    for (const length of [10, 200, 50]) {
      await chrome.alarms.create('a'.repeat(length), createParams);
    }
    const alarms = await chrome.alarms.getAll();
    chrome.test.assertEq(3, alarms.length);
    chrome.test.succeed();
  },
]);
