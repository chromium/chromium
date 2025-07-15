// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `session` doesn't have storage access by default. All other storage areas are
// accessible.
const accessibleStorageAreas = ['sync', 'local'];
const invalidAccessMessage =
    'Error: Access to storage is not allowed from this context.';
const contextCannotSetAccessLevelMessage =
    'Error: Context cannot set the storage access level';

async function testAccessibleStorageAreas(func, param) {
  // `sync` and `local` storage areas have untrusted access level by default,
  // thus content scripts can access the storage.
  for (const area of accessibleStorageAreas) {
    if (param !== undefined) {
      await chrome.storage[area][func](param);
    } else {
      await chrome.storage[area][func]();
    }
  }
}

async function testInaccessibleStorageAreas(func, param) {
  // `session` storage area has only trusted access level by default, thus
  // content scripts cannot access the storage.
  // If the param exists, insert a copy of the param to the area params.
  const areaParams = [];
  if (param !== undefined) {
    areaParams.push(param);
  }
  await chrome.test.assertPromiseRejects(
      chrome.storage.session[func](...areaParams), invalidAccessMessage);
};

async function testGetValueSetByBackgroundPage() {
  // Content scripts can access values set by the background page when the
  // storage area has untrusted access.
  for (const area of accessibleStorageAreas) {
    await new Promise((resolve) => {
      chrome.storage[area].get('background', (value) => {
        chrome.test.assertEq({background: area}, value);
        resolve();
      });
    })
  };
};

chrome.test.runTests([
  // This test must run before any other test clears the storage.
  async function getValueSetByBackgroundPageFromContentScript() {
    // `sync` and `local` storage areas can access background page values by
    // default.
    await testGetValueSetByBackgroundPage();
    // `session` storage area cannot access background page values by
    // default.
    await testInaccessibleStorageAreas('get', 'background');
    chrome.test.succeed();
  },

  async function setValueFromContentScript() {
    await testAccessibleStorageAreas('set', {foo: 'bar'});
    await testInaccessibleStorageAreas('set', {foo: 'bar'});
    chrome.test.succeed();
  },

  async function getValueFromContentScript() {
    await testAccessibleStorageAreas('get', 'foo');
    await testInaccessibleStorageAreas('get', 'foo');
    chrome.test.succeed();
  },

  async function getKeysFromContentScript() {
    await testAccessibleStorageAreas('getKeys');
    await testInaccessibleStorageAreas('getKeys');
    chrome.test.succeed();
  },

  async function getBytesInUseFromContentScript() {
    await testAccessibleStorageAreas('getBytesInUse', null);
    await testInaccessibleStorageAreas('getBytesInUse', null);
    chrome.test.succeed();
  },

  async function removeValueFromContentScript() {
    await testAccessibleStorageAreas('remove', 'foo');
    await testInaccessibleStorageAreas('remove', 'foo');
    chrome.test.succeed();
  },

  async function clearValuesFromContentScript() {
    await testAccessibleStorageAreas('clear');
    await testInaccessibleStorageAreas('clear');
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
        invalidAccessMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.setAccessLevel(
            {accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.local.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    await chrome.test.assertPromiseRejects(
        chrome.storage.sync.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'}),
        contextCannotSetAccessLevelMessage);
    chrome.test.succeed();
  }
]);
