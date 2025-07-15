// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Since the extension set the access level to TRUSTED_AND_UNTRUSTED_CONTEXTS,
// all storage areas are accessible.
const accessibleStorageAreas = ['sync', 'local', 'session', 'managed'];
const contextCannotSetAccessLevelMessage =
    'Error: Context cannot set the storage access level';

async function testAccessibleStorageAreas(func, param) {
  // Content scripts can access the storage when the storage area has untrusted
  // access.
  for (const area of accessibleStorageAreas) {
    if ((func === 'set' || func === 'remove' || func === 'clear') &&
        area === 'managed') {
      await chrome.test.assertPromiseRejects(
          chrome.storage[area][func](param),
          'Error: This is a read-only store.');
    } else {
      // If the param exists and to avoid mutating it, insert a copy of the
      // param to each area params.
      await chrome.storage[area][func](param);
    }
  }
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
    // `sync` and `local` storage areas can always access background page
    // values.
    // `session` storage area with `TRUSTED_AND_UNTRUSTED_CONTEXTS` access level
    // can access background page values.
    // `managed` storage area is read-only, so the background page cannot edit
    // values in it.

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
    // TODO(crbug.com/40189208): `setAccessLevel` is exposed to all valid
    // storage areas, but cannot be accessed from a content script. This will
    // change once we only expose `setAccessLevel` in unprivileged contexts.
    chrome.test.assertTrue(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.local.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.managed.setAccessLevel);
    await chrome.test.assertPromiseRejects(
        chrome.storage.session.setAccessLevel(
            {accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.local.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.sync.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.setAccessLevel(
            {accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    chrome.test.succeed();
  },

]);
