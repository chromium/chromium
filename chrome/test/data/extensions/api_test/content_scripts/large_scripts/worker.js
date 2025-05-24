// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.runTests([
  async function checkContentScriptInjectionResults() {
    async function getTitleForTab(tabId) {
      let results = await chrome.scripting.executeScript(
          {target: {tabId}, func: () => document.title});
      chrome.test.assertEq(1, results.length);
      return results[0].result;
    };

    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);
    const title = await getTitleForTab(tab.id);
    chrome.test.assertEq('I CHANGED TITLE!!!', title);

    // Only inject_element_1.js and change_title.js should be loaded/injected as
    // big.js exceeds the individual script size limit, and loading
    // inject_element_2.js would exceed the extension's total script size limit.
    chrome.test.assertEq(['injected'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
