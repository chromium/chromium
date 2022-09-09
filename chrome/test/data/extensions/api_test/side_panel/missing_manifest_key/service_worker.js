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
    const expected = {};
    const result = await chrome.sidePanel.getOptions({tabId: tabId});
    chrome.test.assertEq(expected, result);
    chrome.test.succeed();
  }
]);
