// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

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
  },

  // Tests that a content script can listen for storage.session.onChanged when
  // session storage allows untrusted access.
  async function onChanged() {
    // Listen for message from content script after it receives the onChanged
    // event.
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      chrome.test.assertEq(message, 'storage.session.onChanged received');
      chrome.test.succeed();
    });

    // Navigate to url where listener_script.js will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    await openTab(url);

    await chrome.storage.session.set({notify: 'yes'});
  },
]);
