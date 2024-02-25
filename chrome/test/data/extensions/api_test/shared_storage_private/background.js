// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic browser tests for the sharedStoragePrivate API.
chrome.test.runTests([
  async function testStorage() {
    // Set values and ensure that get() returns them.
    await chrome.sharedStoragePrivate.set({a:1, b:2});
    let items = await chrome.sharedStoragePrivate.get();
    chrome.test.assertEq(2, Object.keys(items).length);
    chrome.test.assertEq(1, items.a);
    chrome.test.assertEq(2, items.b);

    // Set should merge new values into existing ones.
    await chrome.sharedStoragePrivate.set({b:22, c:3});
    items = await chrome.sharedStoragePrivate.get();
    chrome.test.assertEq(3, Object.keys(items).length);
    chrome.test.assertEq(1, items.a);
    chrome.test.assertEq(22, items.b);
    chrome.test.assertEq(3, items.c);

    // Remove specified keys, unknown keys ignored.
    await chrome.sharedStoragePrivate.remove(['a', 'b', 'x']);
    items = await chrome.sharedStoragePrivate.get();
    chrome.test.assertEq(1, Object.keys(items).length);
    chrome.test.assertEq(3, items.c);

    // Clean up the keys added by the test, so that it will pollute the ash
    // state for the Lacros browser tests.
    await chrome.sharedStoragePrivate.remove(['c']);

    chrome.test.succeed();
  },
]);
