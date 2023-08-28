// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

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
        `Error: Could not load javascript '${scriptFile}' for content script.`);

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
        `Error: Invalid value for 'content_scripts[0].matches[0]': Missing ` +
            `scheme separator.`);

    chrome.test.succeed();
  },

  // Tests that a (valid) script is registered and injected into a frame where
  // the extension has host permissions for.
  async function scriptRegistered_HostPermissions() {
    await chrome.userScripts.unregister();

    const scripts = [{
      id: 'hostPerms',
      matches: ['*://requested.com/*'],
      js: [{file: 'script.js'}],
      runAt: 'document_end'
    }];

    await chrome.userScripts.register(scripts);

    // After the script has been registered, navigate to a url where the script
    // will be injected.
    const config = await chrome.test.getConfig();
    const url = `http://requested.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    // Verify script changed the tab title.
    const currentTab = await chrome.tabs.get(tab.id);
    chrome.test.assertEq('NEW TITLE', currentTab.title);
    chrome.test.succeed();
  },

  // Tests that a registered user script will not be injected into a frame
  // where the extension does not have the host permissions for.
  async function scriptRegistered_NoHostPermissions() {
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
