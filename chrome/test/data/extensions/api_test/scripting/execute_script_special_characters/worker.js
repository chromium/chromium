// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.runTests([
  // Check that a script with special characters in its file name is loaded
  // correctly. Specifically, make sure that the input file name doesn't go
  // through a transformation such as URL encoding.
  async function scriptWithSpecialCharacters() {
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);

    await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, files: ['#script.js']});

    chrome.test.assertEq(['hashtag_script'],
        await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Make sure that there's no special treatment for names that look like a
  // URL-encoded string.
  async function scriptWithEscapedSpecialCharacters() {
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);

    await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, files: ['%23script.js']});

    chrome.test.assertEq(['hashtag_script_escaped'],
        await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Check that given an escaped path for a script that does not exist,
  // executeScript() returns an error, even if a file with the non-escaped name
  // exists.
  async function nonExistentScriptWithEscapedSpecialCharacters() {
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript(
            {target: {tabId: tab.id}, files: ['%23script_2.js']}),
        `Error: Could not load file: '%23script_2.js'.`);

    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
