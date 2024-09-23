// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `session` doesn't have storage access by default. All other storage areas are
// accessible.
const accessibleStorageAreas = ['sync', 'local'];
const invalidAccessMessage =
    'Access to storage is not allowed from this context.';

async function testAccessibleStorageAreas(func, param) {
  // `sync` and `local` storage areas have untrusted access level by default,
  // thus content scripts can access the storage.
  for (const area of accessibleStorageAreas) {
    // If the param exists and to avoid mutating it, insert a copy of the param
    // to each area params.
    let areaParams = param ? [param] : [];
    await new Promise((resolve) => {
      areaParams.push(() => {
        chrome.test.assertNoLastError();
        resolve();
      })
      chrome.storage[area][func](...areaParams);
    })
  };
};

async function testInaccessibleStorageAreas(func, param) {
  // `session` storage area has only trusted access level by default, thus
  // content scripts cannot access the storage.
  // If the param exists, insert a copy of the param to the area params.
  let areaParams = param ? [param] : [];
  await new Promise((resolve) => {
    areaParams.push(() => {
      chrome.test.assertLastError(invalidAccessMessage);
      resolve();
    })
    chrome.storage.session[func](...areaParams);
  })
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

  function setAccessLevelFromContentScript() {
    // `setAccessLevel` is not exposed to `sync` or `local`.
    chrome.test.assertFalse(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertFalse(!!chrome.storage.local.setAccessLevel);
    // TODO(crbug.com/40189208): `setAccessLevel` is exposed to `session` but
    // cannot be accessed from a content script. This will change once we only
    // expose `setAccessLevel` in unprivileged contexts.
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_CONTEXTS'}, () => {
          chrome.test.assertLastError(invalidAccessMessage);
          chrome.test.succeed();
        });
  }
]);
