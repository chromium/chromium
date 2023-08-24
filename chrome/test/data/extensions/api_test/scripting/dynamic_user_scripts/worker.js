// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

function getInjectedElementIds() {
  let childIds = [];
  for (const child of document.body.children)
    childIds.push(child.id);
  return childIds.sort();
};

chrome.test.runTests([
  // Test that unregisterContentScripts unregisters only content scripts and
  // not user scripts.
  async function unregisterScript_CannotUnregisterUserScripts() {
    await chrome.scripting.unregisterContentScripts();

    const contentScripts = [{
      id: 'contentScript',
      matches: ['*://*/*'],
      js: ['content_script.js'],
      runAt: 'document_end'
    }];

    const userScripts = [{
      id: 'userScript',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_end'
    }];


    await chrome.scripting.registerContentScripts(contentScripts);
    await chrome.userScripts.register(userScripts);

    // Navigate to a requested url.
    const config = await chrome.test.getConfig();
    const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);
    let results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: getInjectedElementIds});

    // Both content and user scripts should be injected.
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(
        ['injected_content_script', 'injected_user_script'], results[0].result);

    // Try to unregister user script's id using the scripting API. It should
    // fail because it's not a content script.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.unregisterContentScripts({ids: ['userScript']}),
        `Error: Nonexistent script ID 'userScript'`);

    // Verify all scripts are still registered.
    let registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, registeredContentScripts.length);
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);

    // Re-navigate to the requested url, and verify both scripts are injected.
    tab = await openTab(url);
    results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: getInjectedElementIds});
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(
        ['injected_content_script', 'injected_user_script'], results[0].result);

    // Unregister all content scripts using the scripting API.
    await chrome.scripting.unregisterContentScripts();

    // Verify content script is removed, but user script is still registered.
    registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(0, registeredContentScripts.length);
    registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredUserScripts.length);

    // Re-navigate to the requested url, and verify only the user script is
    // injected.
    tab = await openTab(url);
    results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: getInjectedElementIds});
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(['injected_user_script'], results[0].result);

    chrome.test.succeed();
  },

]);
