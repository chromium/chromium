// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CSS = 'body { background-color: green !important }';
const GREEN = 'rgb(0, 128, 0)';

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
  async function changeBackground() {
    const query = {url: 'http://example.com/*'};
    const tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.insertCSS(
          {
            target: {
              tabId: tab.id,
            },
            css: CSS,
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
            css: CSS,
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
    chrome.test.assertEq(`b.com ${GREEN}`, colors[0]);
    chrome.test.assertEq(`subframes.example ${GREEN}`, colors[1]);
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
          css: CSS,
        },
        results => {
          chrome.test.assertLastError(`No tab with id: ${nonExistentTabId}`);
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
          css: CSS,
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
