// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Inject a script  which changes the page's title based on the execution world
// it's running on, then call executeScript which checks the title.
async function runTest(world, expectedTitle) {
  await chrome.scripting.unregisterContentScripts();
  var scripts = [{
    id: 'script1',
    matches: ['*://hostperms.com/*'],
    js: ['change_title.js'],
    world,
    runAt: 'document_end',
  }];

  await chrome.scripting.registerContentScripts(scripts);
  const config = await chrome.test.getConfig();

  // After the scripts has been registered, navigate to a url where they will be
  // injected.
  const url = `http://hostperms.com:${
      config.testServer.port}/extensions/main_world_script_flag.html`;
  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript({
    target: {tabId: tab.id},
    func: () => document.title,
  });

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(expectedTitle, results[0].result);
  chrome.test.succeed();
}

chrome.test.runTests([
  async function mainWorld() {
    runTest(chrome.scripting.ExecutionWorld.MAIN, 'MAIN_WORLD');
  },

  async function isolatedWorld() {
    runTest(chrome.scripting.ExecutionWorld.ISOLATED, 'ISOLATED_WORLD');
  },
]);
