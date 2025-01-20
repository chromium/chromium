// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';

function getFileTooLargeError(fileName) {
  return `Error: Could not load file: '${fileName}'. Resource size exceeded.`;
}

chrome.test.runTests([
  async function singleScriptExceedsLimit() {
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);

    await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, files: ['small.js']});

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript(
            {target: {tabId: tab.id}, files: ['big.js']}),
        getFileTooLargeError('big.js'));

    chrome.test.assertEq(['small'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  async function totalScriptSizeExceedsLimit() {
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);

    await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, files: ['medium.js']});

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript(
            {target: {tabId: tab.id}, files: ['medium.js', 'small.js']}),
        getFileTooLargeError('small.js'));

    // If an executeScript() call exceeds the script size limit, the call should
    // be a no-op.
    chrome.test.assertEq(['medium'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
