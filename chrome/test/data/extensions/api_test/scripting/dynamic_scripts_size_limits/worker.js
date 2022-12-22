// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

function getInjectedElementIds() {
  let childIds = [];
  for (const child of document.body.children)
    childIds.push(child.id);
  return childIds.sort();
};

function getFileTooLargeError(fileName) {
  return `Error: Scripts could not be loaded because '${fileName}' exceeds ` +
      `the maximum script size or the extension's maximum total script size.`;
}

chrome.test.runTests([
  async function exceedsPerScriptLimitSingleCall() {
    await chrome.scripting.unregisterContentScripts();
    const scriptFile = 'inject_dynamic_too_big.js';
    const scripts = [{
      id: 'too_big',
      matches: ['*://example.com/*'],
      js: [scriptFile],
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        getFileTooLargeError(scriptFile));

    const registeredScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq([], registeredScripts);

    chrome.test.succeed();
  },

  async function exceedsPerExtensionLimitSingleCall() {
    await chrome.scripting.unregisterContentScripts();
    const bigScriptFile = 'inject_dynamic_slightly_big.js';
    const scripts = [
      {
        id: 'too_big',
        matches: ['http://example.com/*'],
        js: [
          'inject_dynamic_1.js',
          'inject_dynamic_2.js',
          bigScriptFile,
        ],
      },
      {
        id: 'too_big_part2',
        matches: ['https://example.com/*'],
        js: [bigScriptFile],
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        getFileTooLargeError(bigScriptFile));

    const registeredScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq([], registeredScripts);

    chrome.test.succeed();
  },

  async function exceedsPerExtensionLimitOverall() {
    await chrome.scripting.unregisterContentScripts();
    const bigScriptFile = 'inject_dynamic_slightly_big.js';
    const scripts = [{
      id: 'too_big',
      matches: ['http://example.com/*'],
      js: [
        'inject_dynamic_1.js',
        bigScriptFile,
        'inject_dynamic_2.js',
      ],
    }];

    // Since the total length of files specified in the script above does not
    // exceed the per-extension limit, the script is registered successfully.
    await chrome.scripting.registerContentScripts(scripts);

    // However, the total length of all scripts for the extension:
    // inject_manifest.js and `scripts` exceed the extension limit, as these
    // scripts are being loaded (manifest first, then dynamic), any file that
    // would exceed the per-extension limit at load-time will not have its
    // contents loaded and will not do anything once injected. In this case,
    // the files that have any actual effect are: inject_manifest.js,
    // inject_dynamic_1.js and inject_dynamic_1.js.
    // inject_dynamic_slightly_big.js will not have its contents loaded and thus
    // will not affect the tab's contents.
    const config = await chrome.test.getConfig();
    const url = `http://example.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);
    let results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: getInjectedElementIds});

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(
        ['dynamic-1', 'dynamic-2', 'manifest'], results[0].result);
    const registeredScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, registeredScripts.length);

    chrome.test.succeed();
  },
]);
