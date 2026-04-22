// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.getConfig(function(config) {
    const args = JSON.parse(config.customArg);
    chrome.test.runTests([
      function queryTabs() {
        chrome.tabs.query(
            {windowId: args.windowId},
            pass(function(tabs) {
              assertEq(tabs.length, args.isIncognito ? 1 : 0);
            }),
        );
      },

      function createTab() {
        chrome.tabs.create(
            {windowId: args.windowId},
            args.isIncognito ? pass(function(tab) {
              assertTrue(tab.incognito);
            }) :
                               fail(`No window with id: ${args.windowId}.`),
        );
      },
    ]);
  });
});
