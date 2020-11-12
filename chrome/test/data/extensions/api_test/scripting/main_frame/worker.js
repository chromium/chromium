// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NEW_TITLE = 'Hello, world!';

function injectedFunction() {
  // NOTE(devlin): We currently need to (re)hard-code this title, since the
  // injected function won't keep the execution context from the surrounding
  // script.
  document.title = 'Hello, world!';
  return document.title;
}

async function getSingleTab(query) {
  const tabs = await new Promise(resolve => {
    chrome.tabs.query(query, resolve);
  });
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

chrome.test.runTests([
  async function changeTitle() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.executeScript(
          {
            target: {
              tabId: tab.id,
            },
            function: injectedFunction,
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE, tab.title);
    chrome.test.succeed();
  },

  async function noSuchTab() {
    const nonExistentTabId = 99999;
    // NOTE(devlin): We can't use a fancy `await` here, because the lastError
    // won't be properly set. This will work better with true promise support,
    // where this could be wrapped in an e.g. expectThrows().
    chrome.scripting.executeScript(
        {
          target: {
            tabId: nonExistentTabId,
          },
          function: injectedFunction,
        },
        async results => {
          chrome.test.assertLastError(`No tab with id: ${nonExistentTabId}`);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function disallowedPermission() {
    const query = {url: 'http://chromium.org/*'};
    let tab = await getSingleTab(query);
    const expectedTitle = 'Title Of Awesomeness';
    chrome.test.assertEq(expectedTitle, tab.title);
    chrome.scripting.executeScript(
        {
          target: {
            tabId: tab.id,
          },
          function: injectedFunction,
        },
        async results => {
          chrome.test.assertLastError(
              `Cannot access contents of url "${tab.url}". ` +
                  'Extension manifest must request permission ' +
                  'to access this host.');
          chrome.test.assertEq(undefined, results);
          tab = await getSingleTab(query);
          chrome.test.assertEq(expectedTitle, tab.title);
          chrome.test.succeed();
        });
  },
]);
