// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { openTab, getInjectedElementIds } from '/_test_resources/test_util/tabs_util.js';

// Scripts that inject a div to the document.
const injectDivScript1 = `var div = document.createElement('div');
                          div.id = 'injected_code_1';
                          document.body.appendChild(div);`;
const injectDivScript2 = `var div = document.createElement('div');
                          div.id = 'injected_code_2';
                          document.body.appendChild(div);`;

// Inject a script which changes the page's title based on the execution world
// it's running on, then call executeScript which checks the title.
async function runExecutionWorldTest(world, expectedTitle) {
  await chrome.userScripts.unregister();

  const scripts = [{
    id: 'us1',
    matches: ['*://*/*'],
    js: [{file: 'inject_to_world.js'}],
    runAt: 'document_end',
    world
  }];
  await chrome.userScripts.register(scripts);
  const config = await chrome.test.getConfig();

  // After the scripts has been registered, navigate to a url where they will be
  // injected.
  const url = `http://requested.com:${
      config.testServer.port}/extensions/main_world_script_flag.html`;
  const tab = await openTab(url);
  chrome.test.assertEq(expectedTitle, tab.title);

  chrome.test.succeed();
}

chrome.test.runTests([
  // Tests that an error is returned when multiple user script entries in
  // userScripts.register share the same ID.
  async function duplicateScriptId_DuplicatesInSameCall() {
    const scriptId = 'script1';

    const scripts = [
      {id: scriptId, matches: ['*://*/*'], js: [{file: 'script.js'}]},
      {id: scriptId, matches: ['*://*/*'], js: [{file: 'script_2.js'}]}
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that if two register calls specifying the same ID are made in quick
  // succession, the first call will successfully register the script and the
  // second call with return with an error.
  async function duplicateScriptId_DuplicateInPendingRegistration() {
    await chrome.userScripts.unregister();

    const scriptId = 'script2';
    const scripts = [
      {id: scriptId, matches: ['*://notused.com/*'], js: [{file: 'script.js'}]}
    ];

    const results = await Promise.allSettled([
      chrome.userScripts.register(scripts), chrome.userScripts.register(scripts)
    ]);

    chrome.test.assertEq('fulfilled', results[0].status);
    chrome.test.assertEq('rejected', results[1].status);
    chrome.test.assertEq(
        `Duplicate script ID '${scriptId}'`, results[1].reason.message);

    chrome.test.succeed();
  },

  // Tests that an error is returned when a user script to be registered has
  // the same ID as a previously registered user script.
  async function duplicateScriptId_DuplicatePreviouslyRegistered() {
    await chrome.userScripts.unregister();

    const scriptId = 'script3';
    const scripts = [
      {id: scriptId, matches: ['*://notused.com/*'], js: [{file: 'script.js'}]}
    ];

    await chrome.userScripts.register(scripts);
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a user script is specified with an
  // invalid ID.
  async function emptyScriptId() {
    await chrome.userScripts.unregister();

    const scripts =
        [{id: '', matches: ['*://notused.com/*'], js: [{file: 'script.js'}]}];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: Script's ID must not be empty`);

    chrome.test.succeed();
  },

  // Test that no scripts are registered when an empty array of scripts is
  // passed to userScripts.register.
  async function emptyScripts() {
    await chrome.userScripts.unregister();

    await chrome.userScripts.register([]);
    const registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(0, registeredUserScripts.length);

    chrome.test.succeed();
  },

  // Test that an error is returned if a user script is specified with a file
  // that cannot be read.
  async function scriptFileError() {
    await chrome.userScripts.unregister();

    const scriptFile = 'nonexistent.js';
    const scripts = [
      {id: 'script4', matches: ['*://notused.com/*'], js: [{file: scriptFile}]}
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: Could not load javascript '${scriptFile}' for script.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a user script does not specify any
  // script source to inject.
  async function invalidScriptSource_EmptyJs() {
    await chrome.userScripts.unregister();

    const scriptId = 'empty';
    const scripts = [{id: scriptId, matches: ['*://notused.com/*'], js: []}];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: User script with ID '${scriptId}' must specify at least one ` +
            `js source.`);
    chrome.test.succeed();
  },

  // Test that an error is returned if a user script source does not specify
  // either code or file.
  async function invalidScriptSource_EmptySource() {
    await chrome.userScripts.unregister();

    const scriptId = 'script5';
    const scripts = [{id: scriptId, matches: ['*://notused.com/*'], js: [{}]}];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: User script with ID '${scriptId}' must specify exactly one ` +
            `of 'code' or 'file' as a js source.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a user script source specifies both
  // code and file.
  async function invalidScriptSource_MultipleSource() {
    await chrome.userScripts.unregister();

    const scriptId = 'script6';
    const scripts = [{
      id: scriptId,
      matches: ['*://notused.com/*'],
      js: [{file: 'script.js', code: ''}]
    }];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: User script with ID '${scriptId}' must specify exactly one ` +
            `of 'code' or 'file' as a js source.`);

    chrome.test.succeed();
  },

  // Test that a user script must specify a list of match patterns.
  async function matchPatternsNotSpecified() {
    await chrome.userScripts.unregister();

    const scriptId = 'script7';
    const scripts = [{id: scriptId, js: [{file: 'script.js'}]}];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: User script with ID '${scriptId}' must specify 'matches'.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a user script specifies a malformed
  // match pattern.
  async function invalidMatchPattern() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'invalidMatchPattern',
      matches: ['invalid**match////'],
      js: [{file: 'script.js'}]
    }];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: Script with ID 'invalidMatchPattern' has invalid value for ` +
            `matches[0]: Missing scheme separator.`);
    chrome.test.succeed();
  },

  async function registeringScriptWithInvalidWorldIdThrowsAnError() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'invalidMatchPattern',
      matches: ['http://example.com/*'],
      js: [{file: 'script.js'}],
      worldId: '_'
    }];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.register(scripts),
        `Error: World IDs beginning with '_' are reserved.`);
    chrome.test.succeed();
  },

  // Tests that a registered user script with files is injected into a frame
  // where the extension has host permissions for and matches the script match
  // patterns.
  async function registerFile_HostPermissions() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'hostPerms',
      matches: ['*://requested.com/*'],
      js: [{ file: 'inject_element.js' }, { file: 'inject_element_2.js' }],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // After the script has been registered, navigate to a url where the script
    // will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify script files were injected.
    chrome.test.assertEq(
      ['injected_user_script', 'injected_user_script_2'],
      await getInjectedElementIds(tab.id));
    chrome.test.succeed();
  },

  // Tests that a registered user script with a file will not be injected into a
  // frame where the extension does not have the host permissions for.
  async function registerFile_NoHostPermissions() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'noHostPerms',
      matches: ['*://non-requested.com/*'],
      js: [{file: 'script.js'}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // After the script has been registered, navigate to a url where the script
    // cannot be injected.
    const config = await chrome.test.getConfig();
    const url =
        `http://non-requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify script didn't change the tab title.
    const currentTab = await chrome.tabs.get(tab.id);
    chrome.test.assertEq('OK', currentTab.title);
    chrome.test.succeed();
  },

  // Tests that a registered user script is injected only when the URL:
  //   - matches any "matches" pattern or matches any "includeGlobs" pattern, if
  //   present
  //   - and does not match "excludeMatches", if present
  //   - and does not match "excludeGlobs" patterns, if present.
  async function registerFile_URLPatterns() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'hostPerms',
      matches: ['*://*.requested.com/*'],
      excludeMatches: ['*://*.excluded.com/*'],
      includeGlobs: ['*include_glob*'],
      excludeGlobs: ['*exclude_glob*'],
      js: [{file: 'script.js'}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);
    const config = await chrome.test.getConfig();

    // Script is NOT injected when URL matches any "excludeGlobs" pattern,
    // regardless if URL also matches any "matches" pattern.
    let url = `http://exclude_glob.requested.com:${
        config.testServer.port}/simple.html`;
    let tab = await openTab(url);
    chrome.test.assertEq('OK', tab.title);

    // Script is NOT injected when URL matches any "excludeMatches" pattern,
    // regardless if URL also matches any "includeGlobs" pattern.
    url = `http://include_glob.excluded.com:${
        config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq('OK', tab.title);

    // Script is NOT injected when URL is not in the extension manifest host
    // permissions, regardless URL matches "includeGlobs" and neither
    // "excludeMatches" or "excludeGLobs".
    url = `http://include_glob.non-requested.com:${
        config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq('OK', tab.title);

    // Script is injected when URL matches any "matches" pattern and doesn't
    // match "excludeMatches" or "excludeGlobs" patterns.
    url = `http://requested.com:${config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq('NEW TITLE', tab.title);

    // Script is injected when URL matches any "includeGlobs" and doesn't match
    // "excludeMatches" or "excludeGlobs" patterns. Note that extension
    // requested host permissions for'requested-2.com' but is not on the script
    // "matches".
    url = `http://include_glob.requested-2.com:${
        config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq('NEW TITLE', tab.title);

    // Script is injected when URL matches a URL "matches" and "includeGlobs"
    // pattern, and doesn't match "excludeMatches" or "excludeGlobs" patterns.
    url = `http://include_glob.requested.com:${
        config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq('NEW TITLE', tab.title);

    chrome.test.succeed();
  },

  // Tests that a file is executed in the correct execution world.
  async function registerFile_ExecutionWorld() {
    runExecutionWorldTest(chrome.userScripts.ExecutionWorld.MAIN, 'MAIN_WORLD');
    runExecutionWorldTest(
        chrome.userScripts.ExecutionWorld.USER_SCRIPT, 'USER_SCRIPT_WORLD');
  },

  // Tests that a registered user script with code is injected into a frame
  // where the extension has host permissions for and URL matches the script
  // match patterns.
  async function registerCode_HostPermissions() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'us1',
      matches: ['*://requested.com/*'],
      js: [{code: injectDivScript1}, {code: injectDivScript2}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // Verify script was registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq('us1', registeredUserScripts[0].id);
    chrome.test.assertEq(
        [{code: injectDivScript1}, {code: injectDivScript2}],
        registeredUserScripts[0].js);

    // After the script has been registered, navigate to a url where the script
    // should be injected.
    const config = await chrome.test.getConfig();
    const url = `http://requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify scripts were injected.
    chrome.test.assertEq(
        ['injected_code_1', 'injected_code_2'],
        await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that a registered user script with code is not injected into a frame
  // where the extension has host permissions for the URL but the URL is not in
  // the script match patterns.
  async function registerCode_HostPermissions_NoMatch() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'us1',
      matches: ['*://matches.com/*'],
      js: [{code: injectDivScript1}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // Verify script was registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq('us1', registeredUserScripts[0].id);

    // After the script has been registered, navigate to a url requested by the
    // extension but not in the script match patterns.
    const config = await chrome.test.getConfig();
    const url = `http://requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify script wasn't injected.
    await chrome.tabs.get(tab.id);
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));
    chrome.test.succeed();
  },

  // Tests that a registered user script with code will not be injected into a
  // frame where the extension does not have the host permissions for.
  async function registerCode_NoHostPermissions() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'us1',
      matches: ['*://non-requested.com/*'],
      js: [{code: 'document.title = \'NEW TITLE\''}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // Verify script was registered.
    let registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq('us1', registeredUserScripts[0].id);

    // After the script has been registered, navigate to a url where the script
    // cannot be injected.
    const config = await chrome.test.getConfig();
    const url =
        `http://non-requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify script didn't change the tab title.
    const currentTab = await chrome.tabs.get(tab.id);
    chrome.test.assertEq('OK', currentTab.title);
    chrome.test.succeed();
  },

  // Tests that a file can be used both as a user script and content script.
  async function fileUsedAsContentScript() {
    await chrome.userScripts.unregister();

    const file = 'script.js'
    const contentScripts =
        [{id: 'contentScript', matches: ['*://*/*'], js: [file]}];
    const userScripts =
        [{id: 'userScript', matches: ['*://*/*'], js: [{file: file}]}];

    await chrome.scripting.registerContentScripts(contentScripts);
    await chrome.userScripts.register(userScripts);

    const registerContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq('contentScript', registerContentScripts[0].id);
    const registeredUserScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq('userScript', registeredUserScripts[0].id);

    chrome.test.succeed();
  },

]);
