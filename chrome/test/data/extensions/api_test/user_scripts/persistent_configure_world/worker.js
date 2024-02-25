// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInjectedElementIds, openTab} from '/_test_resources/test_util/tabs_util.js';

// Error when messaging is not allowed.
const noConnectionError =
    'Error: Could not establish connection. Receiving end does not exist.';

// Script that adds a div when eval() is not allowed.
const evalScript = `let result;
   let div = document.createElement('div');
   try {
     eval('result = "allowed eval"');
     div.id = 'customized_csp';
   } catch (e) {
     div.id = 'extension_csp';
   };
   document.body.appendChild(div); `;


// Script that listens for a message and sends a response.
const receiveMessageScript =
    `if (chrome.runtime?.sendMessage) {
       chrome.runtime.onMessage.addListener(
         (message, sender, sendResponse) => {
         sendResponse(message == 'ping' ? 'pong' : 'Bad message');
       });
     }`;

// Script that updates the title based on messaging availability.
const updateTitleScript =
    `let message = (chrome.runtime?.sendMessage === undefined)
         ? 'messaging disabled' : 'messaging enabled';
     document.title = message;`;

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

// For the first session, register a user script in the USER_SCRIPT world
// (default) with messaging enabled and customized csp.
async function runFirstSession() {
  const userScripts = [{
    id: 'us1',
    matches: ['*://*/*'],
    js: [
      {code: receiveMessageScript}, {code: updateTitleScript},
      {code: evalScript}
    ],
    runAt: 'document_end'
  }];

  await chrome.userScripts.register(userScripts);
  chrome.userScripts.configureWorld(
    { messaging: true, csp: `script-src 'unsafe-eval'` });

  const tab = await navigateToRequestedUrl();

  // Verify user script can send messages.
  chrome.test.assertEq('messaging enabled', tab.title);

  // Verify user script can receive messages.
  let response = await chrome.tabs.sendMessage(tab.id, 'ping');
  chrome.test.assertEq('pong', response);

  // Verify user script uses the customized csp.
  chrome.test.assertEq(['customized_csp'], await getInjectedElementIds(tab.id));

  chrome.test.succeed();
}

// For the second session, verify that the user script registered from the
// first session has messaging enabled. At the end, disable messaging.
async function runSecondSession() {
  // Verify script is registered.
  const userScripts = await chrome.userScripts.getScripts();
  chrome.test.assertEq(1, userScripts.length);

  const tab = await navigateToRequestedUrl();

  // Verify user script can send messages.
  chrome.test.assertEq('messaging enabled', tab.title);

  // Verify user script can receive messages.
  let response = await chrome.tabs.sendMessage(tab.id, 'ping');
  chrome.test.assertEq('pong', response);

  // Verify user script still uses the customized csp.
  chrome.test.assertEq(['customized_csp'], await getInjectedElementIds(tab.id));

  // Disable messaging and csp (by omitting its entry).
  chrome.userScripts.configureWorld({messaging: false});
  chrome.test.succeed();
}

// For the third session, verify that the user script registered from the
// first session has messaging disabled.
async function runThirdSession() {
  // Verify script is registered.
  const userScripts = await chrome.userScripts.getScripts();
  chrome.test.assertEq(1, userScripts.length);

  const tab = await navigateToRequestedUrl();

  // Verify user script cannot send messages.
  chrome.test.assertEq('messaging disabled', tab.title);

  // Verify user script cannot receive messages.
  await chrome.test.assertPromiseRejects(
      chrome.tabs.sendMessage(tab.id, 'ping'), noConnectionError);

  // Verify user script uses the extension's csp.
  chrome.test.assertEq(['extension_csp'], await getInjectedElementIds(tab.id));

  chrome.test.succeed();
}

// Attach a dummy listener for onStartup to kick off the service worker on
// every browser start, so we call chrome.test.sendMessage
chrome.runtime.onStartup.addListener(async () => {});

chrome.test.sendMessage('ready', testName => {
  if (testName === 'PRE_PRE_PersistentWorldConfiguration')
    runFirstSession();
  else if (testName === 'PRE_PersistentWorldConfiguration')
    runSecondSession();
  else if (testName === 'PersistentWorldConfiguration')
    runThirdSession();
  else
    chrome.test.fail();
});
