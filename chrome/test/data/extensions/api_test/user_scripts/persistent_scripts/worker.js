// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

// Returns the injected element ids in `tabId`.
async function getInjectedElementIds(tabId) {
  let injectedElements = await chrome.scripting.executeScript({
    target: {tabId: tabId},
    func: () => {
      let childIds = [];
      for (const child of document.body.children)
        childIds.push(child.id);
      return childIds.sort();
    }
  });
  chrome.test.assertEq(1, injectedElements.length);
  return injectedElements[0].result;
};

// For the first session, register two user scripts.
async function runFirstSession() {
  console.log('runFirstSession');
  const userScripts = [
    {
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_end'
    },
    {
      id: 'us2',
      matches: ['*://*/*'],
      js: [{file: 'user_script_2.js'}],
      runAt: 'document_end'
    }
  ];

  await chrome.userScripts.register(userScripts);
  const tab = await navigateToRequestedUrl();

  // Verify scripts were injected.
  chrome.test.assertEq(
      ['injected_user_script', 'injected_user_script_2'],
      await getInjectedElementIds(tab.id));

  chrome.test.succeed();
}

// For the second session, verify that the user scripts registered from the
// first session are injected. At the end, unregister one of the user scripts
// and register a content script.
async function runSecondSession() {
  console.log('runSecondSession');
  const expectedUserScripts = [
    {
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      allFrames: false,
      runAt: 'document_end'
    },
    {
      id: 'us2',
      matches: ['*://*/*'],
      js: [{file: 'user_script_2.js'}],
      allFrames: false,
      runAt: 'document_end'
    }
  ];

  // Verify scripts are registered.
  const userScripts = await chrome.userScripts.getScripts();
  chrome.test.assertEq(expectedUserScripts, userScripts);

  const tab = await navigateToRequestedUrl();

  // Verify scripts were injected.
  chrome.test.assertEq(
      ['injected_user_script', 'injected_user_script_2'],
      await getInjectedElementIds(tab.id));

  // Add a content script using the scripting API.
  const contentScripts =
      [{id: 'cs1', matches: ['*://*/*'], js: ['content_script.js']}];
  await chrome.scripting.registerContentScripts(contentScripts);

  // Remove one of the user scripts.
  await chrome.userScripts.unregister({ids: ['us2']});
  console.log('finish unregister and runSecondSession() will succeed');

  chrome.test.succeed();
}

// For the third session, verify user script with id us1 and content script are
// both registered and injected.
async function runThirdSession() {
  console.log('runThirdSession');
  const userScripts = await chrome.userScripts.getScripts();
  chrome.test.assertEq(1, userScripts.length);
  chrome.test.assertEq('us1', userScripts[0].id)
  const contentScripts = await chrome.scripting.getRegisteredContentScripts();
  chrome.test.assertEq(1, contentScripts.length);
  chrome.test.assertEq('cs1', contentScripts[0].id)

  // Verify registered scripts are injected
  const tab = await navigateToRequestedUrl();
  chrome.test.assertEq(
      ['injected_content_script', 'injected_user_script'],
      await getInjectedElementIds(tab.id));

  chrome.test.succeed();
}

// Attach a dummy listener for onStartup to kick off the service worker on
// every browser start, so we call chrome.test.sendMessage
chrome.runtime.onStartup.addListener(async () => {});

chrome.test.sendMessage('ready', testName => {
  console.log('chrome.test.sendMessage  with test: ', testName);
  if (testName === 'PRE_PRE_PersistentScripts')
    runFirstSession();
  else if (testName === 'PRE_PersistentScripts')
    runSecondSession();
  else if (testName === 'PersistentScripts')
    runThirdSession();
  else
    chrome.test.fail();
});
