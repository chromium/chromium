// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/491158086): Remove this file along with histogram test.
chrome.test.runTests([
  async function() {
    // Extension has access to Action API.
    chrome.test.assertTrue('action' in chrome);
    chrome.test.assertTrue('setBadgeText' in chrome.action);
    chrome.test.assertTrue('getBadgeText' in chrome.action);
    chrome.test.assertEq('', await chrome.action.getBadgeText({}));

    await chrome.action.setBadgeText({text: 'ABC'});
    chrome.test.assertEq('ABC', await chrome.action.getBadgeText({}));

    await chrome.action.setBadgeText({text: 'A'});
    chrome.test.assertEq('A', await chrome.action.getBadgeText({}));

    await chrome.action.setBadgeText({});
    chrome.test.assertEq('', await chrome.action.getBadgeText({}));

    // Test kApiActionSetBadgeTextByteLimit feature.
    if ((await chrome.test.getConfig()).customArg === 'true') {
      // The call with a string exceeding the length limit rejects.
      await chrome.test.assertPromiseRejects(
          chrome.action.setBadgeText({text: 'A'.repeat(150)}),
          'Error: Badge text size is 150 bytes which exceeds the limit of ' +
              '100 bytes.',
      );
      // The rejected call has no effect on the badge.
      chrome.test.assertEq('', await chrome.action.getBadgeText({}));
    } else {
      // If limit is disabled, call succeeds.
      await chrome.action.setBadgeText({text: 'A'.repeat(150)});
      chrome.test.assertEq(150, (await chrome.action.getBadgeText({})).length);
    }

    chrome.test.succeed();
  },
]);
