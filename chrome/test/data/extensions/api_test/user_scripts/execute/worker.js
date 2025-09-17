// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {findDocumentIdWithHostname, findFrameIdWithHostname, getFramesInTab, getInjectedElementIds, getInjectedElementIdsInOrder, openTab} from '/_test_resources/test_util/tabs_util.js';
import {waitForUserScriptsAPIAllowed} from '/_test_resources/test_util/user_script_test_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

async function navigateToNotRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://not-requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

const injectDivScript = `var div = document.createElement('div');
div.id = 'injected_code_1';
document.body.appendChild(div);`;

const injectDivScript2 = `var div = document.createElement('div');
div.id = 'injected_code_2';
document.body.appendChild(div);`;

// A script that returns "flags" set by scripts in the user script and main
// worlds. Note: We use '<none>' here because undefined and null values aren't
// preserved in return results from userScripts.execute() calls.
const executionWorldFlagsScript = `
      (() => {
        return {
          userScriptWorld: window.userScriptWorldFlag || '<none>',
          mainWorld: window.mainWorldFlag || '<none>',
        };
      })();
    `;

// A script that returns "flags" set by scripts run in specific user script
// worlds.
const worldIdScript = `
      (() => {
        return { worldA: window.worldAFlag || false };
      })();
  `;


chrome.test.runTests([
  waitForUserScriptsAPIAllowed,

  // Tests that an error is returned if the user script source list is empty.
  async function invalidScriptSource_EmptySourceList() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {js: [], target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify at least one js source.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user script source is empty.
  async function invalidScriptSource_EmptySource() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {js: [{}], target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if the user script source specifies both
  // code and file.
  async function invalidScriptSource_MultipleSources() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {
      js: [{file: 'script.js', code: ''}],
      target: {tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user scrip specifies injection to
  // all frames and also a specific set of frame ids.
  async function invalidAllFrames() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {
      js: [{file: 'script.js'}],
      target: {allFrames: true, frameIds: [456], tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot specify 'allFrames' if either 'frameIds' or ` +
            `'documentIds' is specified.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if the user script has both document ids and
  // frame ids as injection targets.
  async function invalidTarget_DocumentIdAndFrameId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {
      js: [{file: 'script.js'}],
      target: {documentIds: ['documentId'], frameIds: [456], tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot specify both 'frameIds' and 'documentIds'.`);

    chrome.test.succeed();
  },

  // Tests that an error is thrown when specifying a non-existent document ID.
  async function invalidTarget_documentId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const frames = await getFramesInTab(tab.id);

    const nonExistentDocumentId = '0123456789ABCDEF0123456789ABCDEF';
    const documentIds = [
      findDocumentIdWithHostname(frames, 'requested.com'),
      nonExistentDocumentId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute({
          js: [{code: `console.log('hello world')`}],
          target: {
            tabId: tab.id,
            documentIds: documentIds,
          }
        }),
        `Error: No document with id ${nonExistentDocumentId} in ` +
            `tab with id ${tab.id}`);

    chrome.test.succeed();
  },

  // Tests that an error is thrown when specifying a non-existent frame ID.
  async function invalidTarget_frameId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const frames = await getFramesInTab(tab.id);

    const nonExistentFrameId = 99999;
    const frameIds = [
      findFrameIdWithHostname(frames, 'requested.com'),
      nonExistentFrameId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute({
          js: [{code: `console.log('hello world')`}],
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          }
        }),
        `Error: No frame with id ${nonExistentFrameId} in ` +
            `tab with id ${tab.id}`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user script has a non-existent tab
  // id.
  async function invalidTarget_tabId() {
    await chrome.userScripts.unregister();

    const tabId = 999;
    const script = {js: [{file: 'script.js'}], target: {tabId: tabId}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script), `Error: No tab with id: ${tabId}`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user script doesn't have site
  // access to the page.
  async function invalidTarget_noSiteAccess() {
    await chrome.userScripts.unregister();

    const tab = await navigateToNotRequestedUrl();
    const script = {js: [{file: 'script.js'}], target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot access contents of the page. Extension manifest must ` +
            `request permission to access the respective host.`);

    chrome.test.succeed();
  },

  async function invalidWorldId_UnderscoreError() {
    await chrome.userScripts.unregister();

    const tab = await navigateToNotRequestedUrl();
    const script = {
      js: [{file: 'script.js'}],
      target: {tabId: tab.id},
      worldId: '_foo'
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: World IDs beginning with '_' are reserved.`);

    chrome.test.succeed();
  },

  async function invalidWorldId_MainWorldError() {
    await chrome.userScripts.unregister();

    const tab = await navigateToNotRequestedUrl();
    const script = {
      js: [{file: 'script.js'}],
      target: {tabId: tab.id},
      world: 'MAIN',
      worldId: '123'
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: World ID can only be specified for USER_SCRIPT worlds.`);

    chrome.test.succeed();
  },

  // Tests that a script with a code source and a valid target is injected.
  async function executeCode() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const script = {js: [{code: injectDivScript}], target: {tabId: tab.id}};
    await chrome.userScripts.execute(script);

    // Verify script was injected.
    chrome.test.assertEq(
        ['injected_code_1'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that a script with a file source and a valid target is injected.
  async function executeFile() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const script = {js: [{file: 'inject_element.js'}], target: {tabId: tab.id}};
    await chrome.userScripts.execute(script);

    // Verify script was injected.
    chrome.test.assertEq(
        ['injected_file_1'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that a script with multiple sources and a valid target is injected
  // in the specified order.
  async function execute_MultipleSources() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const script = {
      js: [
        {code: injectDivScript}, {file: 'inject_element.js'},
        {code: injectDivScript2}
      ],
      target: {tabId: tab.id}
    };
    await chrome.userScripts.execute(script);

    // Verify script was injected.
    chrome.test.assertEq(
        ['injected_code_1', 'injected_file_1', 'injected_code_2'],
        await getInjectedElementIdsInOrder(tab.id));

    chrome.test.succeed();
  },

  // Tests that the script is injected in the corresponding execution world.
  async function executionWorld() {
    await chrome.userScripts.unregister();

    // Navigate to a page with an html file that sets the main world script
    // flag.
    const config = await chrome.test.getConfig();
    const url = `http://requested.com:${
        config.testServer.port}/extensions/main_world_script_flag.html`;
    const tab = await openTab(url);

    // When `world` is unspecified, it defaults to the user script world.
    const defaultWorldScript = {
      js: [{code: `window.userScriptWorldFlag = 'from user script world'`}],
      target: {tabId: tab.id}
    };
    await chrome.userScripts.execute(defaultWorldScript);

    // Executing a script in the user script world that retrieves the world
    // flags should return only the user script world flag (set previously).
    const userWorldScript = {
      js: [{code: executionWorldFlagsScript}],
      target: {tabId: tab.id},
      world: chrome.userScripts.ExecutionWorld.USER_SCRIPT
    };
    let results = await chrome.userScripts.execute(userWorldScript);

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(
        {userScriptWorld: 'from user script world', mainWorld: '<none>'},
        results[0].result);

    // Executing a script in the main world that retrieves the world flags
    // should return only the user script world flag (set by the html file).
    const mainWorldScript = {
      js: [{code: executionWorldFlagsScript}],
      target: {tabId: tab.id},
      world: chrome.userScripts.ExecutionWorld.MAIN
    };
    results = await chrome.userScripts.execute(mainWorldScript);

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(
        {userScriptWorld: '<none>', mainWorld: 'from main world'},
        results[0].result);

    chrome.test.succeed();
  },

  // Tests that the script is injected in the corresponding execution world
  // given a world id.
  async function executionWorldId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    // Set a flag in the user script world A.
    const scriptA_SetVariable = {
      js: [{code: `window.worldAFlag = true`}],
      target: {tabId: tab.id},
      worldId: 'A'
    };
    await chrome.userScripts.execute(scriptA_SetVariable);

    // Executing a script in the user script world A retrieves the world A flag.
    const scriptA_GetVariable = {
      js: [{code: worldIdScript}],
      target: {tabId: tab.id},
      worldId: 'A'
    };
    let results = await chrome.userScripts.execute(scriptA_GetVariable);

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq({worldA: true}, results[0].result);

    // Executing a script in a different world should not retrieve the world A
    // flag.
    const scriptB = {
      js: [{code: worldIdScript}],
      target: {tabId: tab.id},
      worldId: 'B'
    };
    results = await chrome.userScripts.execute(scriptB);

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq({worldA: false}, results[0].result);

    chrome.test.succeed();
  },
])
