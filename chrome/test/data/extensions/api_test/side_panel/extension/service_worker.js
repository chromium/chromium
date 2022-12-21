// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabId;

chrome.test.runTests([
  async function init() {
    const tabs = await chrome.tabs.query({active: true, currentWindow: true});
    tabId = tabs[0].id;
    chrome.test.succeed();
  },

  // Get the manifest panel if setPanel() has never been called.
  async function getPanel() {
    const expected = {enabled: true, path: 'default_path.html'};
    const result = await chrome.sidePanel.getOptions({tabId: tabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Setting incomplete default options should merge with already-set fields
  // from the manifset panel.
  // Regression for crbug.com/1403071.
  async function defaultSetAndGetPanelUpsertManifest() {
    // Disable the default panel.
    await chrome.sidePanel.setOptions({enabled: false});
    let result = await chrome.sidePanel.getOptions({});

    // The manifest path should still be seen when fetching the default panel.
    chrome.test.assertEq({enabled: false, path: 'default_path.html'}, result);

    // Re-enable the default panel without setting a path.
    await chrome.sidePanel.setOptions({enabled: true});
    result = await chrome.sidePanel.getOptions({});

    // The manifest path should still be seen when fetching the default panel.
    chrome.test.assertEq({enabled: true, path: 'default_path.html'}, result);

    chrome.test.succeed();
  },

  // Set panel options by tabId.
  async function setAndGetPanel() {
    const expected = {tabId: tabId, path: 'tab_specific.html', enabled: false};
    await chrome.sidePanel.setOptions(expected);
    const result = await chrome.sidePanel.getOptions({tabId: tabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Set panel options by tabId, for upsert-update test coverage.
  async function setAndGetPanelAgain() {
    const expected = {tabId: tabId, path: 'tab_specific.html', enabled: true};
    await chrome.sidePanel.setOptions(expected);
    const result = await chrome.sidePanel.getOptions({tabId: tabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Set (to add) panel options without a tabId.
  async function defaultSetAndGetPanel() {
    const expected = {path: 'new_default.html', enabled: true};
    await chrome.sidePanel.setOptions(expected);
    const result = await chrome.sidePanel.getOptions({});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Retrieve the newly-set default when we try to fetch the options for a tab
  // ID that doesn't have a dedicated entry.
  async function defaultSetAndGetPanel() {
    const newTabId = tabId + 100;
    const expected = {path: 'new_default.html', enabled: true};
    const result = await chrome.sidePanel.getOptions({tabId: tabId + 100});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Retrieve options for `tabId` after adding default options.
  async function defaultGetPanelForSpecificTab() {
    const expected = {tabId: tabId, path: 'tab_specific.html', enabled: true};
    const result = await chrome.sidePanel.getOptions({tabId: tabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  },

  // Don't set the optional path but set enabled to true.
  async function setEnabledToTrueWithoutAPath() {
    const newTabId = tabId + 200;
    const expected = {tabId: newTabId, enabled: true};
    await chrome.sidePanel.setOptions(expected);
    const result = await chrome.sidePanel.getOptions({tabId: newTabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  }
]);
