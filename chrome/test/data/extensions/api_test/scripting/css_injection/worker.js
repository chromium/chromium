// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CSS_GREEN = 'body { background-color: green !important }';
const GREEN = 'rgb(0, 128, 0)';
const CSS_RED = 'body { background-color: red !important }';
const RED = 'rgb(255, 0, 0)';
const CSS_BLUE = 'body { background-color: blue !important }';
const BLUE = 'rgb(0, 0, 255)';
const CSS_CYAN = 'body { background-color: cyan !important }';
const CYAN = 'rgb(0, 255, 255)';
const YELLOW = 'rgb(255, 255, 0)';

const EXACTLY_ONE_FILE_ERROR = 'Exactly one file must be specified.';

function getBodyColor() {
  const hostname = (new URL(location.href)).hostname;
  return hostname + ' ' + getComputedStyle(document.body).backgroundColor;
}

async function getSingleTab(query) {
  const tabs = await chrome.tabs.query(query);
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

async function getBodyColorsForTab(tabId) {
  const results = await new Promise(resolve => {
    chrome.scripting.executeScript(
        {
          target: {
            tabId: tabId,
            allFrames: true,
          },
          function: getBodyColor,
        },
        resolve);
  });
  chrome.test.assertNoLastError();
  return results.map(res => res.result);
}

chrome.test.runTests([
  // NOTE: These tests re-inject into (potentially) the same frames. This isn't
  // a major problem, because more-recent stylesheets take precedent over
  // previously-inserted ones, but it does put a somewhat unfortunate slight
  // dependency between subtests. If this becomes a problem, we could reload
  // tabs to ensure a "clean slate", but it's not worth the added complexity
  // yet.
  // Instead, each test uses a different color.
  async function changeBackgroundFromString() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.insertCSS(
          {
            target: {
              tabId: tab.id,
            },
            css: CSS_GREEN,
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(1, colors.length);
    chrome.test.assertEq(`example.com ${GREEN}`, colors[0]);
    chrome.test.succeed();
  },

  async function subframes() {
    const query = {url: 'http://subframes.example/*'};
    const tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.insertCSS(
          {
            target: {
              tabId: tab.id,
              allFrames: true,
            },
            css: CSS_RED,
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(2, colors.length);
    colors.sort();
    // Note: injected only in b.com and subframes.example, not c.com (which
    // the extension doesn't have permission to).
    chrome.test.assertEq(`b.com ${RED}`, colors[0]);
    chrome.test.assertEq(`subframes.example ${RED}`, colors[1]);
    chrome.test.succeed();
  },

  async function specificFrames() {
    const query = {url: 'http://subframes.example/*'};
    const tab = await getSingleTab(query);
    const frames = await new Promise(resolve => {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, resolve);
    });
    const bComFrame = frames.find(frame => {
      return (new URL(frame.url)).hostname == 'b.com';
    });
    chrome.test.assertTrue(!!bComFrame);

    const results = await new Promise(resolve => {
      chrome.scripting.insertCSS(
          {
            target: {
              tabId: tab.id,
              frameIds: [bComFrame.frameId],
            },
            css: CSS_BLUE,
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(undefined, results);

    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(2, colors.length);
    colors.sort();
    chrome.test.assertEq(`b.com ${BLUE}`, colors[0]);
    // NOTE: subframes.example frame is still red from the previous test.
    chrome.test.assertEq(`subframes.example ${RED}`, colors[1]);
    chrome.test.succeed();
  },

  async function changeBackgroundFromFile() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.insertCSS(
          {
            target: {
              tabId: tab.id,
            },
            files: ['css_file.css'],
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(undefined, results);
    const colors = await getBodyColorsForTab(tab.id);
    chrome.test.assertEq(1, colors.length);
    chrome.test.assertEq(`example.com ${YELLOW}`, colors[0]);
    chrome.test.succeed();
  },

  async function noSuchTab() {
    const nonExistentTabId = 99999;
    // NOTE(devlin): We can't use a fancy `await` here, because the lastError
    // won't be properly set. This will work better with true promise support,
    // where this could be wrapped in an e.g. expectThrows().
    chrome.scripting.insertCSS(
        {
          target: {
            tabId: nonExistentTabId,
          },
          css: CSS_CYAN,
        },
        results => {
          chrome.test.assertLastError(`No tab with id: ${nonExistentTabId}`);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function noSuchFile() {
    const noSuchFile = 'no_such_file.css';
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    // NOTE(devlin): We can't use a fancy `await` here, because the lastError
    // won't be properly set. This will work better with true promise support,
    // where this could be wrapped in an e.g. expectThrows().
    chrome.scripting.insertCSS(
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
          files: ['css_file.css', 'css_file2.css'],
        },
        results => {
          chrome.test.assertLastError(EXACTLY_ONE_FILE_ERROR);
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },

  async function disallowedPermission() {
    const query = {url: 'http://chromium.org/*'};
    const tab = await getSingleTab(query);
    chrome.scripting.insertCSS(
        {
          target: {
            tabId: tab.id,
          },
          css: CSS_CYAN,
        },
        results => {
          chrome.test.assertLastError(
              `Cannot access contents of url "${tab.url}". ` +
                  'Extension manifest must request permission ' +
                  'to access this host.');
          chrome.test.assertEq(undefined, results);
          chrome.test.succeed();
        });
  },
]);
