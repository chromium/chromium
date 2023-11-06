// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function getRegisteredScripts() {
    // Calling getRegisteredContentScripts with no scripts registered should
    // return an empty array.
    let scripts = await chrome.scripting.getRegisteredContentScripts({});
    chrome.test.assertEq([], scripts);

    const scriptsToRegister = [
      {
        id: 'GRS_1',
        matches: ['*://*/*'],
        excludeMatches: ['*://abc.com/*'],
        css: ['nothing.css'],
        allFrames: true,
        matchOriginAsFallback: true,
      },
      {
        id: 'GRS_2',
        matches: ['*://asdfasdf.com/*'],
        js: ['/dynamic_1.js'],
        runAt: 'document_end',
        persistAcrossSessions: false,
        world: chrome.scripting.ExecutionWorld.MAIN
      }
    ];

    // Some fields are populated with their default values from
    // getRegisteredContentScripts, and file paths are normalized.
    const expectedScripts = [
      {
        id: 'GRS_1',
        matches: ['*://*/*'],
        excludeMatches: ['*://abc.com/*'],
        css: ['nothing.css'],
        allFrames: true,
        runAt: 'document_idle',
        matchOriginAsFallback: true,
        persistAcrossSessions: true,
        world: chrome.scripting.ExecutionWorld.ISOLATED
      },
      {
        id: 'GRS_2',
        matches: ['*://asdfasdf.com/*'],
        js: ['dynamic_1.js'],
        allFrames: false,
        runAt: 'document_end',
        matchOriginAsFallback: false,
        persistAcrossSessions: false,
        world: chrome.scripting.ExecutionWorld.MAIN
      }
    ];

    await chrome.scripting.registerContentScripts(scriptsToRegister);

    // Calling getRegisteredContentScripts with no filter should return all
    // scripts.
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    // Calling getRegisteredContentScripts with ids as a filter should return
    // scripts with matching ids.
    scripts =
        await chrome.scripting.getRegisteredContentScripts({ids: ['GRS_1']});
    chrome.test.assertEq([expectedScripts[0]], scripts);

    // Calling getRegisteredContentScripts with no matching ids as a filter
    // should not return any scripts.
    scripts = await chrome.scripting.getRegisteredContentScripts(
        {ids: ['NONEXISTENT']});
    chrome.test.assertEq([], scripts);

    await chrome.scripting.unregisterContentScripts();
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq([], scripts);

    chrome.test.succeed();
  },
]);
