// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var availableTests = [

  async function testExperimentalActorStartTaskBindings() {
    if (!chrome.experimentalActor || !chrome.experimentalActor.startTask ||
        !chrome.experimentalActor.stopTask ||
        !chrome.experimentalActor.executeAction) {
      chrome.test.fail();
    }

    const empty = new ArrayBuffer([]);
    await chrome.test.assertPromiseRejects(
        chrome.experimentalActor.startTask(empty),
        'Error: Actions API access restricted for this extension.');
    await chrome.test.assertPromiseRejects(
        chrome.experimentalActor.executeAction(empty),
        'Error: Actions API access restricted for this extension.');
    await chrome.test.assertPromiseRejects(
        chrome.experimentalActor.stopTask(1),
        'Error: Actions API access restricted for this extension.');
    chrome.test.succeed();
  },
];

chrome.test.runTests(availableTests);
