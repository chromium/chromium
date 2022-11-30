// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testSetAndGetValue(area) {
  const expectedEntry = {background: area};
  await new Promise((resolve) => {
    chrome.storage[area].set(expectedEntry, resolve);
  });
  const actualEntry = await new Promise((resolve) => {
    chrome.storage[area].get('background', resolve);
  });
  chrome.test.assertEq(expectedEntry, actualEntry);
}

chrome.test.runTests([
  function checkDefaultAccessLevel() {
    // Make sure `setAccessLevel` is only exposed to the `session` storage area.
    chrome.test.assertFalse(!!chrome.storage.local.setAccessLevel);
    chrome.test.assertFalse(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertFalse(!!chrome.storage.managed.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.test.succeed();
  },
  async function setValuesInBackgroundPage() {
    await testSetAndGetValue('session');
    await testSetAndGetValue('local');
    await testSetAndGetValue('sync');
    chrome.test.succeed();
  },
  function allowUntrustedAccessToSessionStorage() {
    // Allow context scripts to access the `session` storage.
    chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'}, () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  }
]);
