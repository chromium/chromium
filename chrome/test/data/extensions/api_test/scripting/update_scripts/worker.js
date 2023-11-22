// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  async function updateScripts() {
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      excludeMatches: ['*://abc.com/*'],
      js: ['inject_element.js'],
      css: ['nothing.css'],
      runAt: 'document_end',
      allFrames: true
    }];

    var updatedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: ['inject_element_2.js'],
      allFrames: false,
      persistAcrossSessions: false
    }];

    await chrome.scripting.registerContentScripts(scripts);
    let tab = await navigateToRequestedUrl();

    // One element with id 'injected' should be injected.
    chrome.test.assertEq(['injected'], await getInjectedElementIds(tab.id));
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, scripts.length);

    await chrome.scripting.updateContentScripts(updatedScripts);
    tab = await navigateToRequestedUrl();

    // After the script is updated, one element with id 'injected_2' should be
    // injected.
    chrome.test.assertEq(['injected_2'], await getInjectedElementIds(tab.id));

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: ['inject_element_2.js'],
      css: ['nothing.css'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: false,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if the script ID specified does not
  // match any registered script and that the failed operation does not change
  // the current set of registered scripts.
  async function updateScriptsNonexistentId() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    const nonexistentScriptId = 'NONEXISTENT';
    var updatedScripts = [{
      id: nonexistentScriptId,
      matches: ['*://hostperms.com/*'],
      js: ['inject_element_2.js'],
      runAt: 'document_idle',
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Script with ID '${
            nonexistentScriptId}' does not exist or is not fully registered`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if more than one entry in the API call
  // contains the same script ID and that the failed operation does not change
  // the current set of registered scripts.
  async function updateScriptsDuplicateIdInAPICall() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'inject_element_1';
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    var updatedScripts = [
      {
        id: scriptId,
        matches: ['*://hostperms.com/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_idle',
      },
      {
        id: scriptId,
        matches: ['*://abc.com/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end',
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Duplicate script ID '${scriptId}'`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if a script is specified with a file
  // that cannot be read.
  async function updateScriptsFileError() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    const scriptFile = 'NONEXISTENT.js';
    var updatedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      js: [scriptFile],
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Could not load javascript '${scriptFile}' for script.`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that if two updateContentScripts calls are made in quick succession,
  // then both calls should succeed in updating their scripts and the old
  // version of these scripts are overwritten.
  // Regression for crbug.com/1454710.
  async function parallelUpdateContentScriptsCalls() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [
      {
        id: 'script_1',
        matches: ['*://*/*'],
        js: ['inject_element.js'],
        runAt: 'document_end',
        allFrames: true
      },
      {
        id: 'script_2',
        matches: ['*://*/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end',
        allFrames: true
      }
    ];

    // First, register 2 scripts that each inject a different element into the
    // page.
    await chrome.scripting.registerContentScripts(scripts);
    let tab = await navigateToRequestedUrl();

    // Both scripts should be injected, and both scripts should inject one
    // element.
    chrome.test.assertEq(
        ['injected', 'injected_2'], await getInjectedElementIds(tab.id));

    // Now update `script_1` and `script_2` to inject different elements.
    const updatedScript1 = [{
      id: 'script_1',
      matches: ['*://*/*'],
      js: ['inject_element_3.js'],
      allFrames: false,
      persistAcrossSessions: false
    }];

    const updatedScript2 = [{
      id: 'script_2',
      matches: ['*://*/*'],
      js: ['inject_element_4.js'],
      allFrames: true,
      persistAcrossSessions: false
    }];

    await Promise.allSettled([
      chrome.scripting.updateContentScripts(updatedScript1),
      chrome.scripting.updateContentScripts(updatedScript2)
    ]);

    tab = await navigateToRequestedUrl();

    // Check that the old versions of both scripts are not injected by checking
    // that the IDs of the elements injected pertain to the updated scripts.
    chrome.test.assertEq(
        ['injected_3', 'injected_4'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
