// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';
import {waitForUserScriptsAPIAllowed} from '/_test_resources/test_util/user_script_test_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  waitForUserScriptsAPIAllowed,

  async function singleScriptExceedsLimit() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    // Execute a script with a valid size.
    const smallScript = {js: [{file: 'small.js'}], target: {tabId: tab.id}};
    await chrome.userScripts.execute(smallScript);

    // Try to execute a script with an invalid size.
    const bigScript = {js: [{file: 'big.js'}], target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(bigScript),
        `Error: Could not load file: 'big.js'. Resource size exceeded.`);

    // Verify only the small script was injected.
    chrome.test.assertEq(['small'], await getInjectedElementIds(tab.id));
    chrome.test.succeed();
  },

  async function totalScriptSizeExceedsLimit() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    // Try to execute a script with two sources that add up to an invalid size.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript(
            {target: {tabId: tab.id}, files: ['medium.js', 'small.js']}),
            `Error: Could not load file: 'small.js'. Resource size exceeded.`);

    // Verify no script was injected.
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
