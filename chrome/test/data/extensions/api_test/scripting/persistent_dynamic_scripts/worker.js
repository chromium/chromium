// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

async function getInjectedElementIds() {
  // We use a setTimeout to ensure any other scripts also injected at
  // `document_idle` have a chance to run.
  await new Promise(resolve => { setTimeout(resolve, 0); });
  let childIds = [];
  for (const child of document.body.children)
    childIds.push(child.id);
  return childIds.sort();
};

// For the first session, register one persistent script and one session script.
async function runFirstSession() {
  let scripts = [
    {
      // A set of the minimal required properties, verifying defaults are
      // properly set and retrieved.
      id: 'minimal_script',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
    },
    {
      // A set of all possible properties, verifying all fields are persisted
      // and retrieved. Note we explicitly set values to non-defaults here to
      // verify the storage.
      id: 'maximal_script',
      matches: ['*://*/*'],
      excludeMatches: ['http://example.com/*'],
      css: ['style.css'],
      js: ['inject_element_2.js'],
      allFrames: true,
      matchOriginAsFallback: true,
      runAt: 'document_end',
      world: chrome.scripting.ExecutionWorld.MAIN,
    },
    {
      // A non-persistent script.
      id: 'non_persistent',
      matches: ['*://*/*'],
      js: ['inject_element_3.js'],
      runAt: 'document_end',
      persistAcrossSessions: false
    }
  ];

  await chrome.scripting.registerContentScripts(scripts);

  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;

  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript(
      {target: {tabId: tab.id}, func: getInjectedElementIds});

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(['injected', 'injected_2', 'injected_3'],
                       results[0].result);

  chrome.test.succeed();
}

// For the second session, verify that the persistent script registered from the
// first session will still be injected into pages. Then replace the persistent
// script. We also register a persistent script, then update it to not persist.
async function runSecondSession() {
  let scripts = await chrome.scripting.getRegisteredContentScripts();

  const expectedScripts = [
    {
      id: 'minimal_script',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      runAt: 'document_idle',
      world: chrome.scripting.ExecutionWorld.ISOLATED,
    },
    {
      id: 'maximal_script',
      matches: ['*://*/*'],
      excludeMatches: ['http://example.com/*'],
      css: ['style.css'],
      js: ['inject_element_2.js'],
      allFrames: true,
      matchOriginAsFallback: true,
      runAt: 'document_end',
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.MAIN,
    },
  ];

  chrome.test.assertEq(expectedScripts, scripts);

  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;

  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript(
      {target: {tabId: tab.id}, func: getInjectedElementIds});

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(['injected', 'injected_2'], results[0].result);

  await chrome.scripting.unregisterContentScripts();

  // Add two new scripts, and then flip their persistence. The IDs indicate
  // the eventual persistent property.
  scripts = [
    {
      id: 'new_script_persistent',
      matches: ['*://*/*'],
      js: ['inject_element_3.js'],
      runAt: 'document_end',
      persistAcrossSessions: false
    },
    {
      id: 'new_script_not_persistent',
      matches: ['*://*/*'],
      js: ['inject_element_4.js']
    },
  ];

  await chrome.scripting.registerContentScripts(scripts);

  const updates = [
    {id: 'new_script_persistent', persistAcrossSessions: true},
    {id: 'new_script_not_persistent', persistAcrossSessions: false}
  ];

  await chrome.scripting.updateContentScripts(updates);
  chrome.test.succeed();
}

// Verify that the removal and addition of a persistent script from the last
// session is applied into this session, and that updating a script's
// persistAcrossSessions flag to false will not persist it into this session.
async function runThirdSession() {
  let scripts = await chrome.scripting.getRegisteredContentScripts();

  const expectedScripts = [
    {
      id: 'new_script_persistent',
      matches: ['*://*/*'],
      js: ['inject_element_3.js'],
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      runAt: 'document_end',
      world: chrome.scripting.ExecutionWorld.ISOLATED,
    },
  ];

  chrome.test.assertEq(expectedScripts, scripts);

  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript(
      {target: {tabId: tab.id}, func: getInjectedElementIds});

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(['injected_3'], results[0].result);

  chrome.test.succeed();
}

// Attach a dummy listener for onStartup to kick off the service worker on
// every browser start, so we call chrome.test.sendMessage
chrome.runtime.onStartup.addListener(async () => {});

chrome.test.sendMessage('ready',testName => {
  if (testName === 'PRE_PRE_PersistentDynamicContentScripts')
    runFirstSession();
  else if (testName === 'PRE_PersistentDynamicContentScripts')
    runSecondSession();
  else if (testName === 'PersistentDynamicContentScripts')
    runThirdSession();
  else
    chrome.test.fail();
});
