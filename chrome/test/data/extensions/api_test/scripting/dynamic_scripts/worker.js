// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function scriptInjected() {
    var scripts = [{
      id: 'script1',
      matches: ['*://*/*'],
      js: ['script.js'],
      runAt: 'document_end'
    }];

    // All that script.js does is send a message, which can be used to verify
    // that the script has been injected.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.test.assertEq('SCRIPT_INJECTED', message);
      chrome.runtime.onMessage.removeListener(passTest);
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
      {id: scriptId, matches: ['*://*/*'], js: ['script.js']},
      {id: scriptId, matches: ['*://*/*'], js: ['script_2.js']}
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
    var scripts = [{id: scriptId, matches: ['*://*/*'], js: ['script.js']}];

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
    const scripts = [{id: scriptId, matches: ['*://*/*'], js: ['script.js']}];

    await chrome.scripting.registerContentScripts(scripts);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function emptyScriptId() {
    const scripts = [{id: '', matches: ['*://*/*'], js: ['script.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Content script's ID must not be empty`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function invalidScriptId() {
    const scriptId = '_manifest_only';
    const scripts = [{id: scriptId, matches: ['*://*/*'], js: ['script.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Content script's ID '${scriptId}' must not start with '_'`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with a
  // file that cannot be read.
  async function scriptFileError() {
    const scriptFile = 'nonexistent.js';
    const scripts = [{id: 'script5', matches: ['*://*/*'], js: [scriptFile]}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Could not load javascript '${scriptFile}' for content script.`);

    chrome.test.succeed();
  },

  // TODO(crbug.com/1215386): Test the following cases:
  //  - Scripts can be registered but not injected into sites where the
  //    extension does not have host permissions for.
  //  - Parsing errors for scripts will return an error.
  //  - Scripts cannot specify empty css/js lists.
  //  - Script files specified by both the extension manifest and dynamic
  //    content scripts will only be injected once.
]);
