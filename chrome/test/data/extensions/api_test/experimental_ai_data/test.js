// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  const {tabId, expectedError} = JSON.parse(config.customArg);
  const capture = options =>
      chrome.experimentalAiData.getApcSnapshot(tabId, options);

  if (expectedError) {
    chrome.test.runTests([
      async function rejectsUnauthorizedCapture() {
        for (const type of ['apc', 'screenshot', 'apc_and_screenshot']) {
          await chrome.test.assertPromiseRejects(
              capture({type}), `Error: ${expectedError}`);
        }
        chrome.test.succeed();
      },
    ]);
    return;
  }

  async function checkSnapshot(options, expectApc, expectScreenshot) {
    const snapshot = await capture(options);
    chrome.test.assertEq(expectApc, 'apcBase64' in snapshot);
    chrome.test.assertEq(expectScreenshot, 'screenshotBase64' in snapshot);
    if (expectApc) {
      // The page's text is embedded in the serialized APC protobuf.
      chrome.test.assertTrue(
          atob(snapshot.apcBase64).includes('APC snapshot browser test'));
    }
    if (expectScreenshot) {
      const bytes = atob(snapshot.screenshotBase64);
      chrome.test.assertEq('\x89PNG\r\n\x1a\n', bytes.slice(0, 8));
      const image = await createImageBitmap(new Blob(
          [Uint8Array.from(bytes, c => c.charCodeAt(0))], {type: 'image/png'}));
      chrome.test.assertTrue(image.width > 0);
      chrome.test.assertTrue(image.height > 0);
      image.close();
    }
  }

  chrome.test.runTests([
    async function defaultsToApc() {
      await checkSnapshot({}, true, false);
      chrome.test.succeed();
    },
    async function capturesApc() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'apc', excludeActionableDetails}, true, false);
      }
      chrome.test.succeed();
    },
    async function capturesScreenshot() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'screenshot', excludeActionableDetails}, false, true);
      }
      chrome.test.succeed();
    },
    async function capturesApcAndScreenshot() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'apc_and_screenshot', excludeActionableDetails}, true, true);
      }
      chrome.test.succeed();
    },
    async function rejectsNegativeMetadataLimit() {
      await chrome.test.assertPromiseRejects(
          capture({maxMetaElements: -1}),
          'Error: maxMetaElements must not be negative.');
      chrome.test.succeed();
    },
    async function rejectsUnknownTab() {
      await chrome.test.assertPromiseRejects(
          chrome.experimentalAiData.getApcSnapshot(999999, {}),
          'Error: Invalid target tab passed in.');
      chrome.test.succeed();
    },
    async function rejectsUnknownSnapshotType() {
      chrome.test.assertThrows(
          () => capture({type: 'unknown'}),
          /Value must be one of apc, apc_and_screenshot, screenshot/);
      chrome.test.succeed();
    },
  ]);
});
