// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

function getInjectedElementIds() {
  let childIds = [];
  for (const child of document.body.children)
    childIds.push(child.id);
  return childIds.sort();
};

// For the first session, register one persistent script and one session script.
async function runFirstSession() {
  let scripts = [
    {
      id: 'inject_element',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      world: chrome.scripting.ExecutionWorld.MAIN
    },
    {
      id: 'inject_element_2',
      matches: ['*://*/*'],
      js: ['inject_element_2.js'],
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
  chrome.test.assertEq(['injected', 'injected_2'], results[0].result);

  chrome.test.succeed();
}

// For the second session, verify that the persistent script registered from the
// first session will still be injected into pages. Then replace the persistent
// script. We also register a persistent script, then update it to not persist.
async function runSecondSession() {
  let scripts = await chrome.scripting.getRegisteredContentScripts();

  const expectedScripts = [{
    id: 'inject_element',
    matches: ['*://*/*'],
    js: ['inject_element.js'],
    allFrames: false,
    runAt: 'document_end',
    matchOriginAsFallback: false,
    persistAcrossSessions: true,
    world: chrome.scripting.ExecutionWorld.MAIN
  }];

  chrome.test.assertEq(expectedScripts, scripts);

  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;

  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript(
      {target: {tabId: tab.id}, func: getInjectedElementIds});

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(['injected'], results[0].result);

  await chrome.scripting.unregisterContentScripts();

  scripts = [
    {
      id: 'inject_element_2',
      matches: ['*://*/*'],
      js: ['inject_element_2.js'],
      runAt: 'document_end',
      persistAcrossSessions: false
    },
    {id: 'inject_element_3', matches: ['*://*/*'], js: ['inject_element.js']}
  ];

  await chrome.scripting.registerContentScripts(scripts);

  const updates = [
    {id: 'inject_element_2', persistAcrossSessions: true},
    {id: 'inject_element_3', persistAcrossSessions: false}
  ];

  await chrome.scripting.updateContentScripts(updates);
  chrome.test.succeed();
}

// Verify that the removal and addition of a persistent script from the last
// session is applied into this session, and that updating a script's
// persistAcrossSessions flag to false will not persist it into this session.
async function runThirdSession() {
  let scripts = await chrome.scripting.getRegisteredContentScripts();

  const expectedScripts = [{
    id: 'inject_element_2',
    matches: ['*://*/*'],
    js: ['inject_element_2.js'],
    allFrames: false,
    runAt: 'document_end',
    matchOriginAsFallback: false,
    persistAcrossSessions: true,
    world: chrome.scripting.ExecutionWorld.ISOLATED
  }];

  chrome.test.assertEq(expectedScripts, scripts);

  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  let results = await chrome.scripting.executeScript(
      {target: {tabId: tab.id}, func: getInjectedElementIds});

  chrome.test.assertEq(1, results.length);
  chrome.test.assertEq(['injected_2'], results[0].result);

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
