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
  // Test that scripts that are unregistered are not injected into a (former)
  // matching frame.
  async function unregisterScripts() {
    var scripts = [
      {
        id: 'inject_element_1',
        matches: ['*://*/*'],
        js: ['inject_element.js'],
        runAt: 'document_end'
      },
      {
        id: 'inject_element_2',
        matches: ['*://*/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end'
      }
    ];

    await chrome.scripting.registerContentScripts(scripts);
    let tab = await navigateToRequestedUrl();

    // Both scripts should be injected, and both scripts should inject one
    // element.
    chrome.test.assertEq(
        ['injected', 'injected_2'], await getInjectedElementIds(tab.id));
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(2, scripts.length);

    await chrome.scripting.unregisterContentScripts(
        {ids: ['inject_element_1']});
    tab = await navigateToRequestedUrl();

    // After removing the script with id 'inject_element_1' and opening a tab,
    // only 'inject_element_2' should be injected.
    chrome.test.assertEq(['injected_2'], await getInjectedElementIds(tab.id));

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, scripts.length);

    chrome.test.succeed();
  },

  // Test that unregisterContentScripts with no given filter unregisters all
  // content scripts.
  async function unregisterScript_NoFilter() {
    await chrome.scripting.unregisterContentScripts();

    const contentScripts = [
      {
        id: 'contentScript1',
        matches: ['*://*/*'],
        js: ['inject_element.js'],
        runAt: 'document_end'
      },
      {
        id: 'contentScript2',
        matches: ['*://*/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end'
      }
    ];

    await chrome.scripting.registerContentScripts(contentScripts);
    let tab = await navigateToRequestedUrl();

    // Verify content scripts are injected.
    chrome.test.assertEq(
        ['injected', 'injected_2'], await getInjectedElementIds(tab.id));

    // Unregister all content scripts.
    await chrome.scripting.unregisterContentScripts();

    // Verify all content scripts are removed.
    let registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(0, registeredContentScripts.length);

    // Re-navigate to the requested url, and verify no script is injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Test that unregisterContentScripts with empty filter ids unregisters all
  // content scripts.
  // TODO(crbug.com/40216362): This is incorrect, when filter ids is empty it
  // should not unregister any script.
  async function unregisterScript_EmptyFilterIds() {
    await chrome.scripting.unregisterContentScripts();

    const contentScripts = [
      {
        id: 'contentScript1',
        matches: ['*://*/*'],
        js: ['inject_element.js'],
        runAt: 'document_end'
      },
      {
        id: 'contentScript2',
        matches: ['*://*/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end'
      }
    ];

    await chrome.scripting.registerContentScripts(contentScripts);
    let tab = await navigateToRequestedUrl();

    // Verify content scripts are injected.
    chrome.test.assertEq(
        ['injected', 'injected_2'], await getInjectedElementIds(tab.id));

    // Unregister all content scripts.
    await chrome.scripting.unregisterContentScripts({ids: []});

    // Verify all content scripts are removed.
    let registeredContentScripts =
        await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(0, registeredContentScripts.length);

    // Re-navigate to the requested url, and verify no script is injected.
    tab = await navigateToRequestedUrl();
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Test that an error is returned when attempting to specify an invalid ID
  // for unregisterContentScripts.
  async function unregisterScriptsWithInvalidID() {
    await chrome.scripting.unregisterContentScripts();

    const scriptId = '_manifest_only';
    await chrome.test.assertPromiseRejects(
        chrome.scripting.unregisterContentScripts({ids: [scriptId]}),
        `Error: Script's ID '${scriptId}' must not start with '_'`);
    chrome.test.succeed();
  },

  // Test that an error is returned when attempting to specify a nonexistent ID
  // for unregisterContentScripts.
  async function unregisterScriptsWithNonexistentID() {
    await chrome.scripting.unregisterContentScripts();

    const validId = 'inject_element_1';
    var scripts = [{
      id: validId,
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end'
    }];

    await chrome.scripting.registerContentScripts(scripts);

    const nonexistentId = 'NONEXISTENT';
    await chrome.test.assertPromiseRejects(
        chrome.scripting.unregisterContentScripts(
            {ids: [validId, nonexistentId]}),
        `Error: Nonexistent script ID '${nonexistentId}'`);

    // UnregisterContentScripts should be a no-op if it fails.
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, scripts.length);
    chrome.test.assertEq(validId, scripts[0].id);

    chrome.test.succeed();
  },
]);
