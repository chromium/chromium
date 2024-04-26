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

async function cleanUpState() {
  // Clear all user script registrations.
  await chrome.userScripts.unregister();

  // Clear all user script world configurations.
  const worldConfigs = await chrome.userScripts.getWorldConfigurations();
  for (const config of worldConfigs) {
    await chrome.userScripts.resetWorldConfiguration(config.worldId);
  }
}

chrome.test.runTests([
  async function UserScriptWorld_worldIdValidation() {
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.configureWorld({csp: '', worldId: ''}),
        'Error: If specified, `worldId` must be non-empty.');
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.configureWorld({csp: '', worldId: '_foobar'}),
        `Error: World IDs beginning with '_' are reserved.`);
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.configureWorld({csp: '', worldId: 'a'.repeat(257)}),
        'Error: World IDs must be at most 256 characters.');
    chrome.test.succeed();
  },

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
    await cleanUpState();

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
    await cleanUpState();

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
    await cleanUpState();

    // Verify all scripts sent messages.
    let hellocount = 0;
    chrome.runtime.onUserScriptMessage.addListener(
        async function listener(message, sender, sendResponse) {
          if (message == 'hello') {
            hellocount++;
          }
          if (hellocount == 3) {
            chrome.runtime.onUserScriptMessage.removeListener(listener);
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
    await cleanUpState();

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
    await cleanUpState();

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

  // Verify that multiple user script worlds are unique and can each have a
  // different CSP specified.
  async function configureWorld_MultipleWorldsAreUnique_Csp() {
    await cleanUpState();

    // Configure "world 1" to allow eval.
    await chrome.userScripts.configureWorld(
        {
          worldId: 'world 1',
          csp: `script-src 'unsafe-eval'`,
        });

    const evalScriptWithWorld =
        `try {
           eval('result = "eval allowed"');
           document.title = document.title + 'WORLD_ID: eval allowed';
         } catch (e) {
           document.title = document.title + 'WORLD_ID: eval disallowed';
         }`;
    const world1Script = evalScriptWithWorld.replaceAll('WORLD_ID', 'world 1');
    const world2Script = evalScriptWithWorld.replaceAll('WORLD_ID', 'world 2');

    // Register two user scripts: one to inject in "world 1" and a second to
    // inject in "world 2".
    const userScripts =
        [
          {
            id: 'us1',
            matches: ['*://*/*'],
            js: [{code: world1Script}],
            worldId: 'world 1',
          },
          {
            id: 'us2',
            matches: ['*://*/*'],
            js: [{code: world2Script}],
            worldId: 'world2',
          },
        ];
    await chrome.userScripts.register(userScripts);

    // Inject the scripts. The first world should allow eval, while the second
    // should not.
    let tab = await navigateToRequestedUrl();
    chrome.test.assertTrue(tab.title.includes('world 1: eval allowed'),
                           tab.title);
    chrome.test.assertTrue(tab.title.includes('world 2: eval disallowed'),
                           tab.title);
    chrome.test.succeed();
  },

  // Verify that multiple user script worlds are unique and can each have a
  // different value for whether messaging APIs are exposed.
  async function configureWorld_MultipleWorldsAreUnique_Messaging() {
    await cleanUpState();

    // Configure "world 1" to allow messaging APIs.
    await chrome.userScripts.configureWorld(
        {
          worldId: 'world 1',
          messaging: true,
        });

    const messagingScriptWithWorld =
        `let titleAddition;
         if (chrome && chrome.runtime && chrome.runtime.sendMessage) {
           titleAddition = 'WORLD_ID: messaging enabled';
         } else {
           titleAddition = 'WORLD_ID: messaging disabled';
         }
         document.title = document.title + titleAddition;`;
    const world1Script =
        messagingScriptWithWorld.replaceAll('WORLD_ID', 'world 1');
    const world2Script =
        messagingScriptWithWorld.replaceAll('WORLD_ID', 'world 2');

    // Register two user scripts: one to inject in "world 1" and a second to
    // inject in "world 2".
    const userScripts =
        [
          {
            id: 'us1',
            matches: ['*://*/*'],
            js: [{code: world1Script}],
            worldId: 'world 1',
          },
          {
            id: 'us2',
            matches: ['*://*/*'],
            js: [{code: world2Script}],
            worldId: 'world2',
          },
        ];
    await chrome.userScripts.register(userScripts);

    // Inject the scripts. The first world should allow messaging, while the
    // second should not.
    let tab = await navigateToRequestedUrl();
    chrome.test.assertTrue(tab.title.includes('world 1: messaging enabled'),
                           tab.title);
    chrome.test.assertTrue(tab.title.includes('world 2: messaging disabled'),
                           tab.title);
    chrome.test.succeed();
  },

  async function canOnlyConfigureUpTo100UserScriptWorlds() {
    await cleanUpState();

    const worldConfigTemplate =
        {
          worldId: 'world #',
          csp: 'test',
        };
    // Register 100 user script world configurations. This should succeed
    // (100 is the current limit).
    for (let i = 0; i < 100; ++i) {
      let worldConfig = { ...worldConfigTemplate };
      worldConfig.worldId += i;
      await chrome.userScripts.configureWorld(worldConfig);
    }

    // Attempting to register one more should fail.
    let worldConfig = { ...worldConfigTemplate };
    worldConfig.worldId += '100';
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.configureWorld(worldConfig),
        'Error: You may only configure up to 100 ' +
            'individual user script worlds.');

    chrome.test.succeed();
  },

  async function canOnlyInjectIn10ActiveWorldsInADocument() {
    await cleanUpState();

    const defaultWorldConfig =
        {
          csp: `script-src 'unsafe-eval'`,
          messaging: true,
        };
    const nonDefaultWorldConfigTemplate =
        {
          csp: `script-src 'self'`,
          messaging: true,
        };
    const injectionTemplate =
        `(() => {
           let result = '<unset>';
           try {
             eval('result = "eval allowed"');
             evalAllowed = 'eval allowed';
           } catch (e) {
             evalAllowed = 'eval disallowed';
           }
           const msg = {worldId: WORLD_ID, result: evalAllowed};
           chrome.runtime.sendMessage('EXT_ID', msg);
         })();`

    // Configure the default world to allow eval.
    await chrome.userScripts.configureWorld(defaultWorldConfig);

    // Returns a zero-padded world ID. Today, scripts are injected in the order
    // defined by their alphabetical ID, rather than by their registration
    // order; this means that a script with ID '10' would inject before ID '2'.
    // We need scripts to inject in a defined order below, so zero-pad the IDs
    // (so it would be 10 vs 02, where 02 injects first).
    // TODO(https://crbug.com/337078958): User scripts should be injected based
    // registration order.
    function getPaddedWorldId(i) {
      return ('' + i).padStart(2, '0');
    }

    // Configure 12 extra worlds, IDs '0' through '11', to not allow eval.
    for (let i = 0; i < 12; ++i) {
      let worldConfig = { ...nonDefaultWorldConfigTemplate };
      worldConfig.worldId = getPaddedWorldId(i);
      await chrome.userScripts.configureWorld(worldConfig);
    }

    // Register 12 user scripts, each injecting into a different world, IDs
    // '0' through '11'.
    let allScripts = [];
    for (let i = 0; i < 12; ++i) {
      const worldId = getPaddedWorldId(i);
      const script = injectionTemplate.replace('WORLD_ID', worldId)
                                      .replace('EXT_ID', chrome.runtime.id);
      const userScript =
          {
            id: 'script-' + worldId,
            matches: ['*://*/*'],
            js: [{code: script}],
            worldId,
          };
      allScripts.push(userScript);
    }
    await chrome.userScripts.register(allScripts);

    let msgCount = 0;
    chrome.runtime.onUserScriptMessage.addListener(
        async function listener(msg) {
          // Each of the first ten scripts (IDs 0 - 9) should have injected into
          // their corresponding world. The 11th and 12th (ID 10 and 11) should
          // have injected into the default world, since we only allow 10 named
          // worlds to be active at a time.
          // We verify which world they injected into by checking whether eval
          // was allowed.
          chrome.test.assertTrue(msg.worldId != undefined);
          chrome.test.assertTrue(msg.worldId < 12);
          const expected =
              msg.worldId < 10 ? 'eval disallowed' : 'eval allowed';
          chrome.test.assertEq(expected, msg.result,
                               'Failed for msg: ' + JSON.stringify(msg));

          ++msgCount;
          if (msgCount == 12) {  // All messages responded.
            chrome.runtime.onUserScriptMessage.removeListener(listener);
            chrome.test.succeed();
          }
        });

    await navigateToRequestedUrl();
  },
]);
