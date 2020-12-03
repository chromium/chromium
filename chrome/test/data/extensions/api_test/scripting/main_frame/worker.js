// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NEW_TITLE_FROM_FUNCTION = 'Hello, world!';
const NEW_TITLE_FROM_FILE = 'Goodnight';
const EXACTLY_ONE_FILE_ERROR = 'Exactly one file must be specified.';

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
  async function changeTitleFromFunction() {
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
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, tab.title);
    chrome.test.succeed();
  },

  async function changeTitleFromFile() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.executeScript(
          {
            target: {
              tabId: tab.id,
            },
            files: ['script_file.js'],
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, tab.title);
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
        results => {
          chrome.test.assertLastError(`No tab with id: ${nonExistentTabId}`);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function noSuchFile() {
    const noSuchFile = 'no_such_file.js';
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    chrome.scripting.executeScript(
        {
          target: {
            tabId: tab.id,
          },
          files: [noSuchFile],
        },
        results => {
          chrome.test.assertLastError(`Could not load file: '${noSuchFile}'.`);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function noFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    chrome.scripting.executeScript(
        {
          target: {
            tabId: tab.id,
          },
          files: [],
        },
        results => {
          chrome.test.assertLastError(EXACTLY_ONE_FILE_ERROR);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function multipleFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    chrome.scripting.executeScript(
        {
          target: {
            tabId: tab.id,
          },
          files: ['script_file.js', 'script_file2.js'],
        },
        results => {
          chrome.test.assertLastError(EXACTLY_ONE_FILE_ERROR);
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
