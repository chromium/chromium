// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Checks chrome.tabs.move end to end from an extension service worker. The
// move semantics (index clamping, pinned tabs, current window) are covered by
// the ExtensionTabsTest.TabsMove* browser tests.

let windowId;
let tabIds;

chrome.test.runTests([
  async function setup() {
    const win = await chrome.windows.create(
        {url: ['about:blank', 'about:blank', 'about:blank']});
    windowId = win.id;
    tabIds = win.tabs.map(tab => tab.id);
    chrome.test.succeed();
  },

  async function moveTab() {
    const tab = await chrome.tabs.move(tabIds[0], {index: 2});
    chrome.test.assertEq(tabIds[0], tab.id);
    chrome.test.assertEq(windowId, tab.windowId);
    chrome.test.assertEq(2, tab.index);
    chrome.test.succeed();
  },

  async function moveMultipleTabs() {
    // [1, 2, 0] -> [0, 2, 1]
    const tabs =
        await chrome.tabs.move([tabIds[0], tabIds[2]], {windowId, index: 0});
    chrome.test.assertEq([tabIds[0], tabIds[2]], tabs.map(tab => tab.id));
    chrome.test.assertEq([0, 1], tabs.map(tab => tab.index));
    chrome.test.succeed();
  },

  function moveToInvalidIndex() {
    chrome.test.assertThrows(
        () => chrome.tabs.move(tabIds[0], {index: -2}),
        'Error in invocation of tabs.move(' +
            '[integer|array] tabIds, object moveProperties, ' +
            'optional function callback): Error at parameter ' +
            '\'moveProperties\': Error at property \'index\': ' +
            'Value must be at least -1.');
    chrome.test.succeed();
  },

  async function moveNonExistentTab() {
    await chrome.test.assertPromiseRejects(
        chrome.tabs.move(999999, {index: 0}), 'Error: No tab with id: 999999.');
    chrome.test.succeed();
  },
]);
