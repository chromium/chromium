// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

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

    // After the script has been registered, navigate to a url where the script
    // will be injected.
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Tests that an error is returned when multiple content script entries in
  // registerContentScripts share the same ID.
  async function duplicateScriptId_DuplicatesInSameCall() {
    await chrome.scripting.unregisterContentScripts();
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
    await chrome.scripting.unregisterContentScripts();
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
    await chrome.scripting.unregisterContentScripts();
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
    await chrome.scripting.unregisterContentScripts();
    const scripts =
        [{id: '', matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script's ID must not be empty`);

    chrome.test.succeed();
  },

  // Test that no scripts are registered when an empty array of scripts is
  // passed to scripting.registerContentScripts().
  async function emptyScripts() {
    await chrome.scripting.unregisterContentScripts();

    await chrome.scripting.registerContentScripts([]);
    let scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(0, scripts.length);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function invalidScriptId() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = '_manifest_only';
    const scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script's ID '${scriptId}' must not start with '_'`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script is specified with a
  // file that cannot be read.
  async function scriptFileError() {
    await chrome.scripting.unregisterContentScripts();
    const scriptFile = 'nonexistent.js';
    const scripts =
        [{id: 'script5', matches: ['*://notused.com/*'], js: [scriptFile]}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Could not load javascript '${scriptFile}' for script.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script does not specify any
  // js/css files to inject.
  async function emptyJSAndCSS() {
    await chrome.scripting.unregisterContentScripts();
    const scripts = [{id: 'empty', matches: ['*://notused.com/*'], css: []}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID 'empty' must specify at least one js or css ` +
            `file.`);
    chrome.test.succeed();
  },

  // Test that a content script must specify a list of match patterns.
  async function matchPatternsNotSpecified() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'matchesNotSpecified';
    const scripts = [{id: scriptId, js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID '${scriptId}' must specify 'matches'.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a malformed
  // match pattern.
  async function invalidMatchPattern() {
    await chrome.scripting.unregisterContentScripts();
    const scripts = [{
      id: 'invalidMatchPattern',
      matches: ['invalid**match////'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID 'invalidMatchPattern' has invalid value for ` +
            `matches[0]: Missing scheme separator.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a disallowed
  // scheme (chrome:// URL).
  async function disallowedMatchPatternSchemeChromeUrl() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'disallowedMatchPatternSchemeChromeUrl';
    const scripts = [{
      id: scriptId,
      matches: ['chrome://newtab/'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID '${scriptId}' has invalid value for `+
            `matches[0]: Invalid scheme.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a disallowed
  // scheme (chrome-extension:// URL).
  async function disallowedMatchPatternSchemeChromeExtensionUrl() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'disallowedMatchPatternSchemeChromeExtensionUrl';
    const scripts = [{
      id: scriptId,
      matches: ['chrome-extension://abcdefghijklmnopabcdefghijklmnop/'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID '${scriptId}' has invalid value for `+
            `matches[0]: Invalid scheme.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a disallowed
  // scheme (isolated-app:// URL).
  async function disallowedMatchPatternSchemeIsolatedAppUrl() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'disallowedMatchPatternSchemeIsolatedAppUrl';
    const scripts = [{
      id: scriptId,
      matches: ['isolated-app://aaaaaaacaibaaaaaaaaaaaaaaiaaeaaaaaaaaaaaaabaeaqaaaaaaaic/'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID '${scriptId}' has invalid value for `+
            `matches[0]: Invalid scheme.`);

    chrome.test.succeed();
  },

  // Test that if `match_origin_as_fallback` is true, any path specified for the
  // script must be wildcarded, otherwise an error is returned.
  async function matchOriginAsFallbackWithPath() {
    await chrome.scripting.unregisterContentScripts();
    let scripts = [{
      id: 'matchOriginAsFallbackWithPath',
      matches: ['https://example/path'],
      matchOriginAsFallback: true,
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: The path component for scripts with ` +
            `'match_origin_as_fallback' must be '*'.`);

    // Try again with a wildcarded path, the register call should succeed.
    scripts[0].matches = ['https://example/*'];
    await chrome.scripting.registerContentScripts(scripts);

    // Test that an error is thrown when attempting to update a script with
    // `match_origin_as_fallback` as true with an invalid path.
    scripts[0].matches = ['https://example/anotherpath'];
    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(scripts),
        `Error: The path component for scripts with ` +
            `'match_origin_as_fallback' must be '*'.`);

    chrome.test.succeed();
  },

  // Test that a registered content script will not be injected into a frame
  // where the extension does not have the host permissions for.
  async function noHostPermissions() {
    await chrome.scripting.unregisterContentScripts();
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
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'hostPerms',
      matches: ['*://hostperms.com/*'],
      js: ['change_title.js'],
      runAt: 'document_end'
    }];

    await chrome.scripting.registerContentScripts(scripts);
    const tab = await navigateToRequestedUrl();
    chrome.test.assertEq('I CHANGED TITLE!!!', tab.title);
    chrome.test.succeed();
  },

  // Test that if the same script file is specified by a manifest content script
  // and a registerContentScripts call, then the script will still only be
  // injected once on a matching frame.
  async function staticAndDynamicScriptInjectedOnce() {
    await chrome.scripting.unregisterContentScripts();
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
  },
]);
