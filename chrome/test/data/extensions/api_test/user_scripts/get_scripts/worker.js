// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that calling getScripts with no filter returns all user scripts.
  async function getScripts_NoFilter() {
    const userScriptsToRegister = [
      {
        id: 'script1',
        matches: ['*://*/*'],
        excludeMatches: ['*://abc.com/*'],
        allFrames: true,
        js: [{file: 'empty.js'}],
        world: chrome.userScripts.ExecutionWorld.MAIN
      },
      {
        id: 'script2',
        matches: ['*://requested.com/*'],
        js: [{file: 'empty2.js'}],
        runAt: 'document_end',
        worldId: 'world 1',
      },
      {
        id: 'script3',
        matches: ['*://requested.com/*'],
        js: [{file: 'empty2.js'}],
        runAt: 'document_idle',
      }
    ];

    const contentScriptsToRegister =
        [{id: 'contentScript', matches: ['*://*/*'], js: ['empty.js']}];

    // Some fields are populated with their default values, and file paths are
    // normalized.
    const expectedUserScripts = [
      {
        id: 'script1',
        matches: ['*://*/*'],
        excludeMatches: ['*://abc.com/*'],
        allFrames: true,
        js: [{file: 'empty.js'}],
        runAt: 'document_idle',
        world: chrome.userScripts.ExecutionWorld.MAIN
      },
      {
        id: 'script2',
        matches: ['*://requested.com/*'],
        js: [{file: 'empty2.js'}],
        allFrames: false,
        runAt: 'document_end',
        world: chrome.userScripts.ExecutionWorld.USER_SCRIPT,
        worldId: 'world 1',
      },
      {
        id: 'script3',
        matches: ['*://requested.com/*'],
        js: [{file: 'empty2.js'}],
        allFrames: false,
        runAt: 'document_idle',
        world: chrome.userScripts.ExecutionWorld.USER_SCRIPT,
      },
    ];

    await chrome.userScripts.register(userScriptsToRegister);
    await chrome.scripting.registerContentScripts(contentScriptsToRegister);

    // Calling getScripts with no filter returns all user scripts.
    let scripts = await chrome.userScripts.getScripts();
    chrome.test.assertEq(expectedUserScripts, scripts);

    // Calling getScripts with an empty filter returns all user scripts.
    scripts = await chrome.userScripts.getScripts({});
    chrome.test.assertEq(expectedUserScripts, scripts);

    chrome.test.succeed();
  },

  // Tests that calling getScripts with empty filter ids returns zero scripts.
  async function getScripts_EmptyFilterIds() {
    await chrome.userScripts.unregister();

    const userScriptsToRegister = [
      {
        id: 'script1',
        matches: ['*://*/*'],
        excludeMatches: ['*://abc.com/*'],
        allFrames: true,
        js: [{file: 'empty.js'}]
      },
      {
        id: 'script2',
        matches: ['*://requested.com/*'],
        js: [{file: 'empty2.js'}],
        runAt: 'document_end'
      }
    ];


    await chrome.userScripts.register(userScriptsToRegister);

    // Calling getScripts with empty ids in filter returns no scripts.
    const scripts = await chrome.userScripts.getScripts({ ids: [] });
    chrome.test.assertEq(0, scripts.length);

    chrome.test.succeed();
  },

  // Tests that calling getScripts with a given filter returns only scripts
  // matching the filter.
  async function getScripts_Filter() {
    await chrome.userScripts.unregister();

    const scriptsToRegister = [
      {id: 'script3', matches: ['*://*/*'], js: [{file: 'empty.js'}]},
      {id: 'script4', matches: ['*://*/*'], js: [{file: 'empty2.js'}]}
    ];

    const expectedScripts = [{
      id: 'script3',
      matches: ['*://*/*'],
      allFrames: false,
      js: [{file: 'empty.js'}],
      runAt: 'document_idle',
      world: chrome.userScripts.ExecutionWorld.USER_SCRIPT
    }];

    await chrome.userScripts.register(scriptsToRegister);

    let scripts =
        await chrome.userScripts.getScripts({ids: ['script3', 'nonExistent']});
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

]);
