// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Error when messaging is not allowed.
const noConnectionError =
    'Error: Could not establish connection. Receiving end does not exist.';

// Script that changes the tab title corresponding to whether eval() is allowed.
const evalScript = `let result;
    try {
      eval('result = "eval allowed"');
      document.title = 'eval allowed';
    } catch (e) {
      document.title = 'eval disallowed';
    }`;

// Script that changes the tab title when messaging is not enabled.
const messagingDisabledScript =
    `let message = (chrome.runtime?.sendMessage === undefined)
         ? 'messaging disabled' : 'messaging present';
       document.title = message;`;

// Script that sends a message, and a second one on response.
const sendMessageScript = `(async function() {
       let response = await chrome.runtime.sendMessage({step: 1});
       let secondMessage = response && response.nextStep
         ? {step: 2} : 'unexpected message';
       chrome.runtime.sendMessage(secondMessage);
       }) ();`;

// Script that listens for a message and sends a response.
const receiveMessageScript = `chrome.runtime.onMessage.addListener(
       (message, sender, sendResponse) => {
         sendResponse(message == 'ping' ? 'pong' : 'Bad message');
       });`;

// Sends a hello message.
const helloScript =
    `if (!!chrome.runtime.sendMessage) { chrome.runtime.sendMessage('hello'); }`

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  // Tests that a registered user script in the USER_SCRIPT world cannot send or
  // receive messages when messaging is disabled.
  async function UserScriptWorld_messagingDisabled() {
    // Register a user script in the USER_SCRIPT world with messaging disabled
    // (default).
    const userScripts = [
      {id: 'us1', matches: ['*://*/*'], js: [{code: messagingDisabledScript}]}
    ];
    await chrome.userScripts.register(userScripts);
    await chrome.userScripts.configureWorld({messaging: false});

    const tab = await navigateToRequestedUrl()

    // Verify user script cannot send messages.
    chrome.test.assertEq('messaging disabled', tab.title);

    // Verify user script cannot receive messages.
    await chrome.test.assertPromiseRejects(
        chrome.tabs.sendMessage(tab.id, 'ping'), noConnectionError);

    chrome.test.succeed();
  },

  // Tests that a registered user script in the USER_SCRIPT world can send or
  // receive messages when messaging is enabled.
  async function UserScriptWorld_messagingEnabled() {
    await chrome.userScripts.unregister();

    // Verify user script can send messages.
    chrome.runtime.onUserScriptMessage.addListener(
        async function listener(message, sender, sendResponse) {
          const url = new URL(sender.url);
          chrome.test.assertEq('requested.com', url.hostname);
          chrome.test.assertEq('/simple.html', url.pathname);
          chrome.test.assertEq(0, sender.frameId);
          chrome.test.assertTrue(!!sender.tab);

          if (message.step == 1) {
            sendResponse({nextStep: true});
          } else {
            chrome.test.assertEq(2, message.step);
            chrome.runtime.onUserScriptMessage.removeListener(listener);
            chrome.test.succeed();
          }
        });

    // Register a user script in the USER_SCRIPT world (default) with messaging
    // enabled.
    let userScripts =
        [{id: 'us1', matches: ['*://*/*'], js: [{code: receiveMessageScript}]}];
    await chrome.userScripts.register(userScripts);
    await chrome.userScripts.configureWorld({messaging: true});

    const tab = await navigateToRequestedUrl();

    // Verify user script can receive messages.
    let response = await chrome.tabs.sendMessage(tab.id, 'ping');
    chrome.test.assertEq('pong', response);

    await chrome.userScripts.unregister();

    // Register a user script that sends a message.
    userScripts =
        [{id: 'us2', matches: ['*://*/*'], js: [{code: sendMessageScript}]}];
    chrome.userScripts.register(userScripts);

    await navigateToRequestedUrl();
  },

  // Tests that a registered user script in the MAIN world cannot send or
  // receive messages when messaging is enabled.
  async function mainWorld_messagingAlwaysDisabled() {
    await chrome.userScripts.unregister();

    // Register a user script in the MAIN world.
    const userScripts = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{code: messagingDisabledScript}],
      world: chrome.userScripts.ExecutionWorld.MAIN
    }];
    await chrome.userScripts.register(userScripts);

    // Enabling USER_SCRIPT world messaging shouldn't not affect user scripts
    // in the MAIN world, which don't have access to messaging APIs.
    chrome.userScripts.configureWorld({messaging: true});

    const tab = await navigateToRequestedUrl()

    // Verify user script cannot send messages.
    chrome.test.assertEq('messaging disabled', tab.title);

    // Verify user script cannot receive messages.
    await chrome.test.assertPromiseRejects(
        chrome.tabs.sendMessage(tab.id, 'ping'), noConnectionError);

    chrome.test.succeed();
  },

  // Test that enabling messaging affects all registered scripts.
  async function messagingEnabled_MultipleScripts() {
    await chrome.userScripts.unregister();

    // Verify all scripts sent messages.
    let hellocount = 0;
    chrome.runtime.onUserScriptMessage.addListener(
        async function listener(message, sender, sendResponse) {
          if (message == 'hello') {
            hellocount++;
          }
          if (hellocount == 3) {
            chrome.test.succeed()
          }
        });

    // Register user scripts in the USER_SCRIPT world (default).
    const userScripts = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{code: helloScript}, {code: helloScript}]
      },
      {id: 'us2', matches: ['*://*/*'], js: [{code: helloScript}]}
    ];
    await chrome.userScripts.register(userScripts);
    await chrome.userScripts.configureWorld({messaging: true});

    // Navigate to a requested page, where the user scripts should send messages
    // back.
    await navigateToRequestedUrl();
  },

  // Tests that configuring the user script world affects scripts registered in
  // the USER_SCRIPT world.
  async function configureCsp_UserScriptWorld() {
    await chrome.userScripts.unregister();

    // Register a user script in the USER_SCRIPT world (default).
    let userScripts =
        [{id: 'us1', matches: ['*://*/*'], js: [{code: evalScript}]}];
    await chrome.userScripts.register(userScripts);

    let tab = await navigateToRequestedUrl()

    // User script defaults to the extension's csp (which disallows eval()).
    chrome.test.assertEq('eval disallowed', tab.title);

    // Update the user script world csp to allow eval().
    await chrome.userScripts.configureWorld({csp: `script-src 'unsafe-eval'`});
    tab = await navigateToRequestedUrl()

    // User script uses the customized csp.
    chrome.test.assertEq('eval allowed', tab.title);

    // Reset the csp. Currently this is only achievable by calling
    // userScripts.configureWorld with no csp value (crbug.com/1497059).
    await chrome.userScripts.configureWorld({});
    tab = await navigateToRequestedUrl()

    // User script eses the extension's csp.
    chrome.test.assertEq('eval disallowed', tab.title);

    chrome.test.succeed();
  },

  // Tests that configuring the user script world does not affect scripts
  // registered in the MAIN world.
  async function configureCsp_MainWorldNotAffected() {
    await chrome.userScripts.unregister();

    // Register a user script in the USER_SCRIPT world (default) with a script
    // that changes the doc title if it can eval() some code.
    let userScripts = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{code: evalScript}],
      world: chrome.userScripts.ExecutionWorld.MAIN
    }];
    await chrome.userScripts.register(userScripts);

    // Update the user script to allow eval().
    await chrome.userScripts.configureWorld({csp: `script-src 'unsafe-eval'`});

    // Navigate to a site whose page csp disallows eval().
    const config = await chrome.test.getConfig();
    const url =
        `http://requested.com:${config.testServer.port}/csp-script-src.html`;
    let tab = await openTab(url);

    // User script in the MAIN wold should use the page csp.
    chrome.test.assertEq('eval disallowed', tab.title);

    chrome.test.succeed();
  },


]);
