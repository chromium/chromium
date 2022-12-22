// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.runTests([
  async function contentScriptsInMainWorld() {
    const config = await chrome.test.getConfig();

    // Opens a tab where a script may be injected that changes the title based
    // on the execution world it's running in, then call executeScript which
    // checks the title.
    const exampleUrl = `http://main.example:${
        config.testServer.port}/extensions/main_world_script_flag.html`;
    let tab = await openTab(exampleUrl);
    let results = await chrome.scripting.executeScript({
      target: {tabId: tab.id},
      func: () => document.title,
    });
    chrome.test.assertEq('MAIN_WORLD', results[0].result);
    chrome.test.succeed();
  },

  async function contentScriptsInIsolatedWorld() {
    const config = await chrome.test.getConfig();

    // Repeat the test above except the script should be running in the isolated
    // world.
    const hostPermsUrl = `http://isolated.example:${
        config.testServer.port}/extensions/main_world_script_flag.html`;
    const tab = await openTab(hostPermsUrl);
    const results = await chrome.scripting.executeScript({
      target: {tabId: tab.id},
      func: () => document.title,
    });
    chrome.test.assertEq('ISOLATED_WORLD', results[0].result);
    chrome.test.succeed();
  },
]);
