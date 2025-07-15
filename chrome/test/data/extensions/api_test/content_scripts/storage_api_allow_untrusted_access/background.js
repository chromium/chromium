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

  async function allowUntrustedAccessToSessionStorage() {
    // Allow context scripts to access the `session` storage.
    await chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    chrome.test.succeed();
  },

  async function allowUntrustedAccessToSyncStorage() {
    // Allow context scripts to access the `sync` storage.
    await chrome.storage.sync.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    chrome.test.succeed();
  },

  async function allowUntrustedAccessToLocalStorage() {
    // Allow context scripts to access the `local` storage.
    await chrome.storage.local.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    chrome.test.succeed();
  },

  async function allowUntrustedAccessToManagedStorage() {
    // Allow context scripts to access the `managed` storage.
    await chrome.storage.managed.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    chrome.test.succeed();
  },

  // Tests that a content script can listen for storage.session.onChanged,
  // storage.sync.onChanged, and storage.local.onChanged when these storage
  // areas allow untrusted access.
  async function onChanged() {
    // Ensure content script can access all relevant storage areas.
    await chrome.storage.session.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    await chrome.storage.sync.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    await chrome.storage.local.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});
    await chrome.storage.managed.setAccessLevel(
        {accessLevel: 'TRUSTED_AND_UNTRUSTED_CONTEXTS'});

    const expectedMessages = [
      'storage.session.onChanged received', 'storage.sync.onChanged received',
      'storage.local.onChanged received'
    ];
    const receivedMessages = new Set();

    // Listen for messages from content script after it receives the onChanged
    // events.
    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      chrome.test.assertTrue(
          expectedMessages.includes(message), `Unexpected message:
          ${message}`);
      receivedMessages.add(message);
      if (receivedMessages.size === expectedMessages.length) {
        chrome.test.succeed();
      }
    });

    // Navigate to url where listener_script.js will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    await openTab(url);

    await chrome.test.assertPromiseRejects(
        chrome.storage.managed.set({notify: 'yes'}),
        'Error: This is a read-only store.');
    await chrome.storage.session.set({notify: 'yes'});
    await chrome.storage.sync.set({notify: 'yes'});
    await chrome.storage.local.set({notify: 'yes'});
  },
]);
