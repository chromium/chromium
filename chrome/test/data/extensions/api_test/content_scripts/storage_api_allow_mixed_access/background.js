// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

async function testSetAndGetValue(area) {
  const expectedEntry = {background: area};
  await chrome.storage[area].set(expectedEntry);
  const actualEntry = await chrome.storage[area].get('background');
  chrome.test.assertEq(expectedEntry, actualEntry);
}

chrome.test.runTests([
  function checkDefaultAccessLevel() {
    // Make sure `setAccessLevel` is exposed to all storage areas.
    chrome.test.assertTrue(!!chrome.storage.local.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.sync.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.session.setAccessLevel);
    chrome.test.assertTrue(!!chrome.storage.managed.setAccessLevel);
    chrome.test.succeed();
  },

  async function setValuesInBackgroundPage() {
    await testSetAndGetValue('session');
    await testSetAndGetValue('local');
    await testSetAndGetValue('sync');
    chrome.test.succeed();
  },

  // Tests that a content script only receives onChanged events for storage
  // areas with untrusted access.
  async function onChanged() {
    await chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_CONTEXTS'});
    await chrome.storage.local.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    await chrome.storage.sync.setAccessLevel({accessLevel: 'TRUSTED_CONTEXTS'});
    await chrome.storage.managed.setAccessLevel(
        {accessLevel: 'TRUSTED_CONTEXTS'});

    // Listen for a message from the content script. We only expect one from the
    // 'local' storage area.
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      chrome.test.assertEq('storage.local.onChanged received', message);
      chrome.test.succeed();
    });

    // Navigate to url where listener_script.js will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    await openTab(url);

    // Trigger onChanged events. The listener should only act on the `local` and
    // `sync` events. Because `sync` has trusted-only access, the listener
    // should not receive the event, and thus only send a message for `local`.
    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.set({notify: 'yes'}),
        'Error: This is a read-only store.');
    await chrome.storage.session.set({notify: 'yes'});
    await chrome.storage.sync.set({notify: 'yes'});
    await chrome.storage.local.set({notify: 'yes'});
  },

]);
