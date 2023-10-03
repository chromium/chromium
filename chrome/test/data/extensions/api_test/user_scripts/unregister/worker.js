// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { openTab, getInjectedElementIds } from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  // Tests that calling unregister with specific ids unregisters such scripts
  // and does not inject them into a (former) matching frame.
  async function unregister_Filter() {
    const userScripts = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        runAt: 'document_end'
      },
      {
        id: 'us2',
        matches: ['*://*/*'],
        js: [{file: 'user_script_2.js'}],
        runAt: 'document_end'
      }
    ];

    await chrome.userScripts.register(userScripts);

    // Verify user scripts were registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(2, registeredUserScripts.length);

    let tab = await navigateToRequestedUrl();

    // Verify both user scripts are injected.
    chrome.test.assertEq(
        ['injected_user_script', 'injected_user_script_2'],
        await getInjectedElementIds(tab.id));

    await chrome.userScripts.unregister({ids: ['us1']});

    // Verify only one script is registered.
    registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);

    // Re-navigate to the requested url, and verify only script with id `us2` is
    // injected.
    tab = await navigateToRequestedUrl(tab.id);
    chrome.test.assertEq(
        ['injected_user_script_2'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that calling unregister with no filter unregisters all user scripts.
  async function unregister_NoFilter() {
    await chrome.userScripts.unregister();

    const userScripts = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        runAt: 'document_end'
      },
      {
        id: 'us2',
        matches: ['*://*/*'],
        js: [{file: 'user_script_2.js'}],
        runAt: 'document_end'
      }
    ];

    await chrome.userScripts.register(userScripts);

    // Verify user scripts were registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(2, registeredUserScripts.length);

    let tab = await navigateToRequestedUrl();

    // Verify user scripts are injected.
    chrome.test.assertEq(
        ['injected_user_script', 'injected_user_script_2'],
        await getInjectedElementIds(tab.id));

    // Unregister all user scripts.
    await chrome.userScripts.unregister();

    // Verify all user scripts are removed.
    registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(0, registeredUserScripts.length);

    // Re-navigate to the requested url, and verify no user script is injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that calling unregister with an empty filter ids doesn't unregister
  // any script.
  async function unregister_EmptyFilterIds() {
    await chrome.userScripts.unregister();

    const userScripts = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        runAt: 'document_end'
      },
    ];

    await chrome.userScripts.register(userScripts);

    let tab = await navigateToRequestedUrl();

    // Verify user script is injected.
    chrome.test.assertEq(
        ['injected_user_script'], await getInjectedElementIds(tab.id));

    // Providing empty ids to unregister causes no script to be removed.
    await chrome.userScripts.unregister({ids: []});

    // Verify user script is still registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);

    // Re-navigate to the requested url, and verify user script is injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq(
        ['injected_user_script'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that an error is returned when attempting to specify an invalid ID
  // for unregister.
  async function unregister_WithInvalidID() {
    await chrome.userScripts.unregister();

    const scriptId = '_id';
    await chrome.test.assertPromiseRejects(
        chrome.scripting.unregisterContentScripts({ids: [scriptId]}),
        `Error: Script's ID '${scriptId}' must not start with '_'`);
    chrome.test.succeed();
  },

  // Tests that an error is returned when attempting to specify a nonexistent ID
  // for unregister.
  async function unregister_ScriptsWithNonexistentID() {
    await chrome.userScripts.unregister();

    const validId = 'us1';
    const userScripts = [
      {
        id: validId,
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        runAt: 'document_end'
      },
    ];


    await chrome.userScripts.register(userScripts);

    const nonexistentId = 'NONEXISTENT';
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.unregister({ids: [validId, nonexistentId]}),
        `Error: Nonexistent script ID '${nonexistentId}'`);

    // unregister should be a no-op if it fails.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);
    chrome.test.assertEq(validId, registeredUserScripts[0].id);

    chrome.test.succeed();
  },

  // Test that unregister removes only user scripts and not content scripts.
  async function unregister_CannotUnregisterContentScripts() {
    await chrome.userScripts.unregister();

    const userScripts = [{
      id: 'userScript',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_end'
    }];

    const contentScripts = [{
      id: 'contentScript',
      matches: ['*://*/*'],
      js: ['content_script.js'],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(userScripts);
    await chrome.scripting.registerContentScripts(contentScripts);

    let tab = await navigateToRequestedUrl();

    // Both content and user scripts should be injected.
    chrome.test.assertEq(
        ['injected_content_script', 'injected_user_script'],
        await getInjectedElementIds(tab.id));

    // Try to unregister content script's id using the user scripts API. It
    // should fail because it's not a user script.
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.unregister({ids: ['contentScript']}),
        `Error: Nonexistent script ID 'contentScript'`);

    // Verify all scripts are still registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);
    let registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, registeredContentScripts.length);

    // Re-navigate to the requested url, and verify both scripts are injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq(
        ['injected_content_script', 'injected_user_script'],
        await getInjectedElementIds(tab.id));

    // Unregister all user scripts using the user scripts API.
    await chrome.userScripts.unregister();

    // Verify user script is removed, but content script is still registered.
    registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(0, registeredUserScripts.length);
    registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, registeredContentScripts.length);

    // Re-navigate to the requested url, and verify only the content script is
    // injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq(
        ['injected_content_script'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

]);
