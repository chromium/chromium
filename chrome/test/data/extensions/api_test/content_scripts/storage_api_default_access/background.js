// Copyright 2021 The Chromium Authors
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
  async function setValuesInBackgroundPage() {
    await testSetAndGetValue('session');
    await testSetAndGetValue('local');
    await testSetAndGetValue('sync');
    chrome.test.succeed();
  },

  // Tests that a content script cannot listen for storage.session.onChanged
  // when session storage only allows trusted access.
  async function onChanged() {
    const expectedMessages =
        ['storage.sync.onChanged received', 'storage.local.onChanged received'];
    const receivedMessages = new Set();

    // Listen for messages from content script after it receives the onChanged
    // events.
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      chrome.test.assertTrue(
          expectedMessages.includes(message), `Unexpected message: ${message}`);
      receivedMessages.add(message);
      if (receivedMessages.size === expectedMessages.length) {
        chrome.test.succeed();
      }
    });

    // Navigate to url where listener_script.js will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    await openTab(url);

    // Check that setting a value for the 'managed' storage area fails, as it
    // is a read-only store.
    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.set({notify: 'yes'}),
        'Error: This is a read-only store.');

    // Setting a value in storage session shouldn't notify the content script
    // when access level only allows trusted access.
    await chrome.storage.session.set({notify: 'yes'});

    // Setting a value in sync and local storage should notify the content
    // script, since access level doesn't affect session storage (see
    // crbug.com/1508463) and has "trusted and untrusted" access by default. We
    // do this to check session.onChanged didn't fire by checking if
    // local.onChanged fired, since events should be received in FIFO order.
    await chrome.storage.sync.set({notify: 'yes'});
    await chrome.storage.local.set({notify: 'yes'});
  }
]);
