// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/492555224): Remove this file along with histogram test.
chrome.test.runTests([
  async function() {
    // If title is not set dynamically or explicitly in manifest, it defaults
    // to the extension's manifest `name` key value.
    const extensionName = chrome.runtime.getManifest().name;
    const invalidTitleError =
        /Error at parameter 'details': Missing required property 'title'./;

    // Extension has access to Action API.
    chrome.test.assertTrue('action' in chrome);
    chrome.test.assertTrue('setTitle' in chrome.action);
    chrome.test.assertTrue('getTitle' in chrome.action);

    chrome.test.assertEq(extensionName, await chrome.action.getTitle({}));
    // Increments 0-byte string bucket.
    await chrome.action.setTitle({title: ''});
    chrome.test.assertEq('', await chrome.action.getTitle({}));

    // Increments 1-byte string bucket.
    await chrome.action.setTitle({title: 'A'});
    chrome.test.assertEq('A', await chrome.action.getTitle({}));

    // Invalid titles throw and do not alter the histogram.
    chrome.test.assertThrows(
        chrome.action.setTitle, null, [{title: null}], invalidTitleError);
    chrome.test.assertThrows(
        chrome.action.setTitle, null, [{title: undefined}], invalidTitleError);
    chrome.test.assertEq('A', await chrome.action.getTitle({}));

    // Increments 60-byte string bucket.
    const testTitle =
        'Action Title Histogram Test — Test all the titles every day!';
    await chrome.action.setTitle({title: testTitle});
    chrome.test.assertEq(testTitle, await chrome.action.getTitle({}));

    // Increments 150-byte string bucket.
    await chrome.action.setTitle({title: 'A'.repeat(150)});
    chrome.test.assertEq(150, (await chrome.action.getTitle({})).length);

    chrome.test.succeed();
  },
]);
