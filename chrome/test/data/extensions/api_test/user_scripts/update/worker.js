// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://hostperms-a.com:${config.testServer.port}/simple.html`;
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

chrome.test.runTests([
  // Test that an error is returned when any of the script IDs specified in
  // userScripts.update do not match any registered user script, and that
  // the failed operation does not change the previously registered user
  // scripts.
  async function nonExistentIdError() {
    await chrome.userScripts.unregister();

    // Register a user script.
    const existentId = 'existentId'
    const scriptsToRegister = [
      {id: existentId, matches: ['*://*/*'], js: [{file: 'user_script.js'}]}
    ];
    await chrome.userScripts.register(scriptsToRegister);

    // Updating scripts when one of them has a non-existent id should fail.
    const nonExistentId = 'nonExistentId'
    const scriptsToUpdate = [
      {
        id: existentId,
        matches: ['*://hostperms-a.com/*'],
        js: [{file: 'user_script_2.js'}]
      },
      {
        id: nonExistentId,
        matches: ['*://hostperms-a.com/*'],
        js: [{file: 'user_script_3.js'}]
      }
    ];
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.update(scriptsToUpdate),
        `Error: Script with ID '${
            nonExistentId}' does not exist or is not fully registered`);

    // Verify previously registered user script was not affected.
    const expectedScripts = [{
      id: existentId,
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_idle',
      allFrames: false,
      world: 'USER_SCRIPT'
    }];
    const registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(expectedScripts, registeredScripts);

    chrome.test.succeed();
  },

  // Test that an error is returned if more than one entry specified in
  // userScripts.update contains the same script ID, and that the failed
  // operation does not change the existent registered user scripts.
  async function duplicateScriptIdError() {
    await chrome.userScripts.unregister();

    // Register a user script.
    const scriptId = 'us1'
    const scriptsToRegister =
        [{id: scriptId, matches: ['*://*/*'], js: [{file: 'user_script.js'}]}];
    await chrome.userScripts.register(scriptsToRegister);

    // Updating two scripts with the same ID should fail.
    const scriptsToUpdate = [
      {
        id: scriptId,
        matches: ['*://hostperms-a.com/*'],
        js: [{file: 'user_script_2.js'}]
      },
      {
        id: scriptId,
        matches: ['*://abc.com/*'],
        js: [{file: 'user_script_2.js'}]
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.update(scriptsToUpdate),
        `Error: Duplicate script ID '${scriptId}'`);

    // Verify previously registered user script was not affected.
    const expectedScripts = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_idle',
      allFrames: false,
      world: 'USER_SCRIPT'
    }];
    const registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(expectedScripts, registeredScripts);

    chrome.test.succeed();
  },

  // Test that an error is returned if any script is specified with a file that
  // cannot be read in userScripts.update, and that the failed operation does
  // not changed the existent registered user scripts.
  async function fileNonExistentError() {
    await chrome.userScripts.unregister();

    // Register user script.
    const scriptsToRegister = [
      {id: 'us1', matches: ['*://*/*'], js: [{file: 'user_script.js'}]},
      {id: 'us2', matches: ['*://*/*'], js: [{file: 'user_script_2.js'}]}
    ];
    await chrome.userScripts.register(scriptsToRegister);
    let tab = await navigateToRequestedUrl();

    // Verify user scripts were registered and injected.
    let registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(2, registeredScripts.length);
    chrome.test.assertEq(
        ['injected_user_script', 'injected_user_script_2'],
        await getInjectedElementIds(tab.id));

    // Updating a script with a non existent file should fail.
    const nonExistentFile = 'NONEXISTENT.js';
    const scriptsToUpdate = [
      {
        id: 'us1',
        matches: ['*://hostperms-a.com/*'],
        js: [{file: 'user_script_3.js'}]
      },
      {
        id: 'us2',
        matches: ['*://hostperms-a.com/*'],
        js: [{file: nonExistentFile}]
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.update(scriptsToUpdate),
        `Error: Could not load javascript '${nonExistentFile}' for script.`);

    // Verify previously registered user scripts were not affected.
    registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(2, registeredScripts.length);
    chrome.test.assertEq(
        ['injected_user_script', 'injected_user_script_2'],
        await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  async function updatingToAnInvalidWorldIdThrowsError() {
    await chrome.userScripts.unregister();

    // Register user script.
    const scriptToRegister = [
      {id: 'us1', matches: ['*://*/*'], js: [{file: 'user_script.js'}]},
    ];
    await chrome.userScripts.register(scriptToRegister);

    // Updating a script with an invalid world ID should fail.
    const scriptUpdate = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        worldId: '_',
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.update(scriptUpdate),
        `Error: World IDs beginning with '_' are reserved.`);

    chrome.test.succeed();
  },

  // Tests that calling userScripts.update with a specific ID updates such
  // script and does not inject them into a (former) matching frame.
  async function scriptUpdated() {
    await chrome.userScripts.unregister();

    // Register user script.
    const scriptsToRegister = [{
      id: 'us1',
      matches: ['*://hostperms-a.com/*'],
      excludeMatches: ['*://abc.com/*'],
      js: [{file: 'user_script.js'}],
      runAt: 'document_end',
      allFrames: true
    }];
    await chrome.userScripts.register(scriptsToRegister);

    // Verify user script was registered.
    let registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredScripts.length);

    // Verify script file is injected in a matching url.
    const config = await chrome.test.getConfig();
    let url = `http://hostperms-a.com:${config.testServer.port}/simple.html`;
    let tab = await openTab(url);
    chrome.test.assertEq(
        ['injected_user_script'], await getInjectedElementIds(tab.id));

    // Update user script matches and javascript file.
    var scriptsToUpdate = [{
      id: 'us1',
      matches: ['*://hostperms-b.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: [{file: 'user_script_2.js'}],
      allFrames: false,
      worldId: 'another world',
    }];
    await chrome.userScripts.update(scriptsToUpdate);

    // Verify user script was updated.
    const expectedScripts = [{
      id: 'us1',
      matches: ['*://hostperms-b.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: [{file: 'user_script_2.js'}],
      runAt: 'document_end',
      allFrames: false,
      world: 'USER_SCRIPT',
      worldId: 'another world',
    }];
    registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(expectedScripts, registeredScripts);

    // Verify no script file is injected in a non-matching url (which previously
    // matched).
    url = `http://hostperms-a.com:${config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq([], await getInjectedElementIds(tab.id));

    // Verify new script file is injected in the new matching url.
    url = `http://hostperms-b.com:${config.testServer.port}/simple.html`;
    tab = await openTab(url);
    chrome.test.assertEq(
        ['injected_user_script_2'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },

  // Tests that calling userScripts.update with a specific ID updates such
  // script and injects the script in the corresponding world.
  async function scriptUpdated_World() {
    await chrome.userScripts.unregister();

    // Register user script with a file that changes the document title based on
    // its execution world.
    const scriptsToRegister = [{
      id: 'us1',
      matches: ['*://hostperms-a.com/*'],
      js: [{file: 'inject_to_world.js'}],
      world: 'MAIN'
    }];
    await chrome.userScripts.register(scriptsToRegister);

    // Verify user script was registered.
    let registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(1, registeredScripts.length);

    // Verify script file is injected in the main world.
    const config = await chrome.test.getConfig();
    const url = `http://hostperms-a.com:${
        config.testServer.port}/extensions/main_world_script_flag.html`;
    let tab = await openTab(url);
    chrome.test.assertEq('MAIN_WORLD', tab.title);

    // Update user script world.
    var scriptsToUpdate =
        [{id: 'us1', js: [{file: 'inject_to_world.js'}], world: 'USER_SCRIPT'}];
    await chrome.userScripts.update(scriptsToUpdate);

    // Verify user script was updated.
    const expectedScripts = [{
      id: 'us1',
      matches: ['*://hostperms-a.com/*'],
      js: [{file: 'inject_to_world.js'}],
      runAt: 'document_idle',
      allFrames: false,
      world: 'USER_SCRIPT'
    }];
    registeredScripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(expectedScripts, registeredScripts);

    // Verify script file is injected in the user script world.
    tab = await openTab(url);
    chrome.test.assertEq('USER_SCRIPT_WORLD', tab.title);

    chrome.test.succeed();
  },

  // Test that if two userScript.update calls are made in quick succession,
  // then both calls should succeed in updating their scripts and the old
  // version of these scripts are overwritten.
  async function scriptUpdated_ParallelCalls() {
    await chrome.userScripts.unregister();

    // Register two user scripts that each inject a different element into the
    // page.
    var scriptsToRegister = [
      {
        id: 'us1',
        matches: ['*://*/*'],
        js: [{file: 'user_script.js'}],
        runAt: 'document_end',
        allFrames: true
      },
      {
        id: 'us2',
        matches: ['*://*/*'],
        js: [{file: 'user_script_2.js'}],
        runAt: 'document_end',
        allFrames: true
      }
    ];
    await chrome.userScripts.register(scriptsToRegister);
    let tab = await navigateToRequestedUrl();

    // Verify both scripts are injected, and each injected one element.
    chrome.test.assertEq(
        ['injected_user_script', 'injected_user_script_2'],
        await getInjectedElementIds(tab.id));

    // Now update both scripts to inject different elements.
    let scriptToUpdate1 = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script_3.js'}],
      allFrames: false
    }];
    let scriptToUpdate2 = [{
      id: 'us2',
      matches: ['*://*/*'],
      js: [{file: 'user_script_4.js'}],
      allFrames: true
    }];

    await Promise.allSettled([
      chrome.userScripts.update(scriptToUpdate1),
      chrome.userScripts.update(scriptToUpdate2)
    ]);
    tab = await navigateToRequestedUrl();

    // Verify that the old versions of both scripts are not injected by checking
    // that the IDs of the elements injected pertain to the updated scripts.
    chrome.test.assertEq(
        ['injected_user_script_3', 'injected_user_script_4'],
        await getInjectedElementIds(tab.id));

    // Now update one script twice to inject different elements.
    scriptToUpdate1 = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script.js'}],
      allFrames: false
    }];
    scriptToUpdate2 = [{
      id: 'us1',
      matches: ['*://*/*'],
      js: [{file: 'user_script_2.js'}],
      allFrames: true
    }];

    await Promise.allSettled([
      chrome.userScripts.update(scriptToUpdate1),
      chrome.userScripts.update(scriptToUpdate2)
    ]);
    tab = await navigateToRequestedUrl();

    // Verify that the double updated script only injects the file from the
    // second update. Note that the other script is still injected, since it
    // wasn't updated.
    chrome.test.assertEq(
        ['injected_user_script_2', 'injected_user_script_4'],
        await getInjectedElementIds(tab.id));


    chrome.test.succeed();
  }
]);
