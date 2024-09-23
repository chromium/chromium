// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Since the extension set the access level to TRUSTED_AND_UNTRUSTED_CONTEXTS,
// all storage areas are accessible.
const accessibleStorageAreas = ['sync', 'local', 'session'];

async function testAccessibleStorageAreas(func, param) {
  // Content scripts can access the storage when the storage area has untrusted
  // access.
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

async function testGetValueSetByBackgroundPage() {
  // Content scripts can access values set by the background page when the
  // storage area has untrusted access. Requires that the background page
  // previously stored {background: '`area`'} for each respective `area`.
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
    // `sync` and `local` storage areas can always access background page
    // values. `session` storage area with `TRUSTED_AND_UNTRUSTED_CONTEXTS`
    // access level can access background page values.
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

  function setAccessLevelFromContentScript() {
    // `setAccessLevel` is not exposed to `sync` or `local`.
    chrome.test.assertFalse(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertFalse(!!chrome.storage.local.setAccessLevel);
    // TODO(crbug.com/40189208): `setAccessLevel` is exposed to `session` but
    // cannot be accessed from a content script. This will change once we
    // expose only `setAccessLevel` in privileged contexts.
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_CONTEXTS'}, () => {
          chrome.test.assertLastError(
              'Context cannot set the storage access level');
          chrome.test.succeed();
        });
  },
]);
