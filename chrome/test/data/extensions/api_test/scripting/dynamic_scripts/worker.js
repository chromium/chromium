// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.runTests([
  async function scriptInjected() {
    var scripts = [{
      id: 'script1',
      matches: ['*://a.com/*'],
      js: ['dynamic_1.js'],
      runAt: 'document_end'
    }];

    // All that dynamic_1.js does is send a message, which can be used to verify
    // that the script has been injected.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('SCRIPT_INJECTED', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // will be injected.
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Tests that an error is returned when multiple content script entries in
  // registerContentScripts share the same ID.
  async function duplicateScriptId_DuplicatesInSameCall() {
    const scriptId = 'script2';

    var scripts = [
      {id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']},
      {id: scriptId, matches: ['*://notused.com/*'], js: ['inject_element.js']}
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that if two registerContentScripts calls specifying the same ID are
  // made in quick succession, the first call will successfully register the
  // script and the second call with return with an error.
  async function duplicateScriptId_DuplicateInPendingRegistration() {
    const scriptId = 'script3';
    var scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    const results = await Promise.allSettled([
      chrome.scripting.registerContentScripts(scripts),
      chrome.scripting.registerContentScripts(scripts)
    ]);

    chrome.test.assertEq('fulfilled', results[0].status);
    chrome.test.assertEq('rejected', results[1].status);
    chrome.test.assertEq(
        `Duplicate script ID '${scriptId}'`, results[1].reason.message);

    chrome.test.succeed();
  },

  // Tests that an error is returned when a content script to be registered has
  // the same ID as a loaded content script.
  async function duplicateScriptId_DuplicatePreviouslyRegistered() {
    const scriptId = 'script4';
    const scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.scripting.registerContentScripts(scripts);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function emptyScriptId() {
    const scripts =
        [{id: '', matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Content script's ID must not be empty`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function invalidScriptId() {
    const scriptId = '_manifest_only';
    const scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Content script's ID '${scriptId}' must not start with '_'`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script is specified with a
  // file that cannot be read.
  async function scriptFileError() {
    const scriptFile = 'nonexistent.js';
    const scripts =
        [{id: 'script5', matches: ['*://notused.com/*'], js: [scriptFile]}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Could not load javascript '${scriptFile}' for content script.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script does not specify any
  // js/css files to inject.
  async function emptyJSAndCSS() {
    const scripts = [{id: 'empty', matches: ['*://notused.com/*'], css: []}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: At least one js or css file is required for ` +
            `'content_scripts[0]'.`);
    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a malformed
  // match pattern.
  async function invalidMatchPattern() {
    const scripts = [{
      id: 'invalidMatchPattern',
      matches: ['invalid**match////'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Invalid value for 'content_scripts[0].matches[0]': Missing ` +
            `scheme separator.`);

    chrome.test.succeed();
  },

  // Test that a registered content script will not be injected into a frame
  // where the extension does not have the host permissions for.
  async function noHostPermissions() {
    var scripts = [{
      id: 'noHostPerms',
      matches: ['*://nohostperms.com/*'],
      js: ['change_title.js'],
      runAt: 'document_end'
    }];

    // check_title.js (manifest content script, for which host permissions do
    // not apply), is run at document_idle. The test passes if the document
    // title sent by check_title.js matches the expected title.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('DOCUMENT_TITLE: OK', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();
    const url = `http://nohostperms.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Test that a registered content script WILL be injected into a frame
  // where the extension has host permissions for.
  async function hostPermissions() {
    var scripts = [{
      id: 'hostPerms',
      matches: ['*://hostperms.com/*'],
      js: ['change_title.js'],
      runAt: 'document_end'
    }];

    async function getTitleForTab(tabId) {
      let results = await chrome.scripting.executeScript(
          {target: {tabId}, func: () => document.title});
      chrome.test.assertEq(1, results.length);
      return results[0].result;
    };

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();
    const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);
    const title = await getTitleForTab(tab.id);

    chrome.test.assertEq('I CHANGED TITLE!!!', title);
    chrome.test.succeed();
  },

  // Test that if the same script file is specified by a manifest content script
  // and a registerContentScripts call, then the script will still only be
  // injected once on a matching frame.
  async function staticAndDynamicScriptInjectedOnce() {
    var scripts = [{
      id: 'inject_element',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end'
    }];

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // matches, but should not be injected.
    const url = `http://b.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);
    const results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: () => document.body.childElementCount});

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(1, results[0].result);
    chrome.test.succeed();
  }
]);
