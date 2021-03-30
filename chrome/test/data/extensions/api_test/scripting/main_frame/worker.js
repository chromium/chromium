// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NEW_TITLE_FROM_FUNCTION = 'Hello, world!';
const NEW_TITLE_FROM_FILE = 'Goodnight';
const EXACTLY_ONE_FILE_ERROR = 'Error: Exactly one file must be specified.';

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
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      function: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, tab.title);
    chrome.test.succeed();
  },

  async function changeTitleFromFile() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      files: ['script_file.js'],
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, tab.title);
    chrome.test.succeed();
  },

  async function injectedFunctionReturnsNothing() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      // Note: This function has no return statement; in JS, this means
      // the return value will be undefined.
      function: () => {},
    });
    chrome.test.assertEq(1, results.length);
    // NOTE: Undefined results are mapped to null in our bindings layer,
    // because they converted from empty base::Values in the same way.
    // NOTE AS WELL: We use `val === null` (rather than
    // `assertEq(null, val)` because assertEq will classify null and undefined
    // as equal.
    chrome.test.assertTrue(results[0].result === null);
    chrome.test.succeed();
  },

  async function injectedFunctionReturnsNull() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      function: () => {
        return null;
      },
    });
    chrome.test.assertEq(1, results.length);
    // NOTE: We use `val === null` (rather than `assertEq(null, val)` because
    // assertEq will classify null and undefined as equal.
    chrome.test.assertTrue(results[0].result === null);
    chrome.test.succeed();
  },

  async function injectedFunctionHasError() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      // This will throw a runtime error, since foo, bar, and baz aren't
      // defined.
      function: () => {
        foo.bar = baz;
        return 3;
      },
    });

    // TODO(devlin): Currently, we don't pass the error from the injected
    // script back to the extension in any way. It'd be helpful to pass
    // this along to the extension.
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(null, results[0].result);
    chrome.test.succeed();
  },

  async function noSuchTab() {
    const nonExistentTabId = 99999;
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: nonExistentTabId,
          },
          function: injectedFunction,
        }),
        `Error: No tab with id: ${nonExistentTabId}`);
    chrome.test.succeed();
  },

  async function noSuchFile() {
    const noSuchFile = 'no_such_file.js';
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: [noSuchFile],
        }),
        `Error: Could not load file: '${noSuchFile}'.`);
    chrome.test.succeed();
  },

  async function noFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: [],
        }),
        EXACTLY_ONE_FILE_ERROR);
    chrome.test.succeed();
  },

  async function multipleFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: ['script_file.js', 'script_file2.js'],
        }),
        EXACTLY_ONE_FILE_ERROR);
    chrome.test.succeed();
  },

  async function disallowedPermission() {
    const query = {url: 'http://chromium.org/*'};
    let tab = await getSingleTab(query);
    const expectedTitle = 'Title Of Awesomeness';
    chrome.test.assertEq(expectedTitle, tab.title);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          function: injectedFunction,
        }),
        `Error: Cannot access contents of url "${tab.url}". ` +
            'Extension manifest must request permission ' +
            'to access this host.');
    tab = await getSingleTab(query);
    chrome.test.assertEq(expectedTitle, tab.title);
    chrome.test.succeed();
  },
]);
