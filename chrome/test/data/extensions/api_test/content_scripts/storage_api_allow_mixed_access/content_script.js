// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The extension set the access level to TRUSTED_AND_UNTRUSTED_CONTEXTS for the
// `local` storage area, while setting `sync`, `session`, and `managed` to the
// `TRUSTED_CONTEXTS` access level.
const accessibleStorageAreas = ['local'];
const invalidAccessMessage =
    'Error: Access to storage is not allowed from this context.';

async function testAccessibleStorageAreas(func, param) {
  // Content scripts can access the storage when the storage area has untrusted
  // access.
  for (const area of accessibleStorageAreas) {
    // If the param exists and to avoid mutating it, insert a copy of the param
    // to each area params.
    await chrome.storage[area][func](param);
  };
};

async function testGetValueSetByBackgroundPage() {
  // Content scripts can access values set by the background page when the
  // storage area has untrusted access. Requires that the background page
  // previously stored {background: '`area`'} for each respective `area`.
  for (const area of accessibleStorageAreas) {
    // The 'managed' storage area is read-only so the background page cannot
    // set a value for it.
    if (area === 'managed') {
      continue;
    }
    const value = await chrome.storage[area].get('background');
    chrome.test.assertEq({background: area}, value);
  };
};

chrome.test.runTests([
  // This test must run before any other test clears the storage.
  async function getValueSetByBackgroundPageFromContentScript() {
    await testGetValueSetByBackgroundPage();
    chrome.test.succeed();
  },

  async function setValueFromContentScript() {
    await testAccessibleStorageAreas('set', {foo: 'bar'});
    chrome.test.succeed();
  },

  async function getValueFromContentScript() {
    await testAccessibleStorageAreas('get', 'foo');
    chrome.test.succeed();
  },

  async function getBytesInUseFromContentScript() {
    await testAccessibleStorageAreas('getBytesInUse', null)
    chrome.test.succeed();
  },

  async function removeValueFromContentScript() {
    await testAccessibleStorageAreas('remove', 'foo');
    chrome.test.succeed();
  },

  async function clearValuesFromContentScript() {
    await testAccessibleStorageAreas('clear')
    chrome.test.succeed();
  },

  async function setAccessLevelFromContentScript() {
    // TODO(crbug.com/40189208): `setAccessLevel` is exposed to `session`,
    // `local`, and `sync` but cannot be accessed from a content script. This
    // will change once we expose only `setAccessLevel` in privileged contexts.
    chrome.test.assertTrue(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.local.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.managed.setAccessLevel);
    await chrome.test.assertPromiseRejects(
        chrome.storage.session.setAccessLevel(
            {accessLevel: 'TRUSTED_CONTEXTS'}),
        invalidAccessMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.local.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        'Error: Context cannot set the storage access level');
    await chrome.test.assertPromiseRejects(
        chrome.storage.sync.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        invalidAccessMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.setAccessLevel(
            {accessLevel: 'TRUSTED_CONTEXTS'}),
        invalidAccessMessage);
    chrome.test.succeed();
  },

  async function accessSessionStorageGetShouldFailIfTrustedAccessOnly() {
    // Validate that setting the access level for other namespaces doesn't "open
    // up" session storage. Session storage is configured for
    // TRUSTED_CONTEXTS_ONLY in background.js.
    await chrome.test.assertPromiseRejects(
        chrome.storage.session.get('background'), invalidAccessMessage);
    chrome.test.succeed();
  },
]);
