// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://hostperms.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

// Returns the injected element ids in `tabId`.
async function getInjectedElementIds(tabId) {
  let injectedElements = await chrome.scripting.executeScript({
    target: { tabId: tabId },
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

async function getTitleForTab(tabId) {
  let results = await chrome.scripting.executeScript(
      {target: {tabId}, func: () => document.title});
  chrome.test.assertEq(1, results.length);
  return results[0].result;
};

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

  async function scriptInjected() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'script1',
      matches: ['*://a.com/*'],
      js: ['dynamic_1.js'],
      runAt: 'document_end'
    }];

    // All that dynamic_1.js does is send a message, which can be used to verify
    // that the script has been injected.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('SCRIPT_INJECTED', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // will be injected.
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Tests that an error is returned when multiple content script entries in
  // registerContentScripts share the same ID.
  async function duplicateScriptId_DuplicatesInSameCall() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'script2';

    var scripts = [
      {id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']},
      {id: scriptId, matches: ['*://notused.com/*'], js: ['inject_element.js']}
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that if two registerContentScripts calls specifying the same ID are
  // made in quick succession, the first call will successfully register the
  // script and the second call with return with an error.
  async function duplicateScriptId_DuplicateInPendingRegistration() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'script3';
    var scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    const results = await Promise.allSettled([
      chrome.scripting.registerContentScripts(scripts),
      chrome.scripting.registerContentScripts(scripts)
    ]);

    chrome.test.assertEq('fulfilled', results[0].status);
    chrome.test.assertEq('rejected', results[1].status);
    chrome.test.assertEq(
        `Duplicate script ID '${scriptId}'`, results[1].reason.message);

    chrome.test.succeed();
  },

  // Tests that an error is returned when a content script to be registered has
  // the same ID as a loaded content script.
  async function duplicateScriptId_DuplicatePreviouslyRegistered() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'script4';
    const scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.scripting.registerContentScripts(scripts);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Duplicate script ID '${scriptId}'`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function emptyScriptId() {
    await chrome.scripting.unregisterContentScripts();
    const scripts =
        [{id: '', matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script's ID must not be empty`);

    chrome.test.succeed();
  },

  // Test that no scripts are registered when an empty array of scripts is
  // passed to scripting.registerContentScripts().
  async function emptyScripts() {
    await chrome.scripting.unregisterContentScripts();

    await chrome.scripting.registerContentScripts([]);
    let scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(0, scripts.length);

    chrome.test.succeed();
  },

  // Tests that an error is returned if a content script is specified with an
  // invalid ID.
  async function invalidScriptId() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = '_manifest_only';
    const scripts =
        [{id: scriptId, matches: ['*://notused.com/*'], js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script's ID '${scriptId}' must not start with '_'`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script is specified with a
  // file that cannot be read.
  async function scriptFileError() {
    await chrome.scripting.unregisterContentScripts();
    const scriptFile = 'nonexistent.js';
    const scripts =
        [{id: 'script5', matches: ['*://notused.com/*'], js: [scriptFile]}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Could not load javascript '${scriptFile}' for content script.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script does not specify any
  // js/css files to inject.
  async function emptyJSAndCSS() {
    await chrome.scripting.unregisterContentScripts();
    const scripts = [{id: 'empty', matches: ['*://notused.com/*'], css: []}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: At least one js or css file is required for ` +
            `'content_scripts[0]'.`);
    chrome.test.succeed();
  },

  // Test that a content script must specify a list of match patterns.
  async function matchPatternsNotSpecified() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'matchesNotSpecified';
    const scripts = [{id: scriptId, js: ['dynamic_1.js']}];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID '${scriptId}' must specify 'matches'`);

    chrome.test.succeed();
  },

  // Test that an error is returned if a content script specifies a malformed
  // match pattern.
  async function invalidMatchPattern() {
    await chrome.scripting.unregisterContentScripts();
    const scripts = [{
      id: 'invalidMatchPattern',
      matches: ['invalid**match////'],
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: Script with ID 'invalidMatchPattern' has invalid value for ` +
            `matches[0]: Missing scheme separator.`);

    chrome.test.succeed();
  },

  // Test that if `match_origin_as_fallback` is true, any path specified for the
  // script must be wildcarded, otherwise an error is returned.
  async function matchOriginAsFallbackWithPath() {
    await chrome.scripting.unregisterContentScripts();
    let scripts = [{
      id: 'matchOriginAsFallbackWithPath',
      matches: ['https://example/path'],
      matchOriginAsFallback: true,
      js: ['dynamic_1.js']
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.registerContentScripts(scripts),
        `Error: The path component for scripts with ` +
            `'match_origin_as_fallback' must be '*'.`);

    // Try again with a wildcarded path, the register call should succeed.
    scripts[0].matches = ['https://example/*'];
    await chrome.scripting.registerContentScripts(scripts);

    // Test that an error is thrown when attempting to update a script with
    // `match_origin_as_fallback` as true with an invalid path.
    scripts[0].matches = ['https://example/anotherpath'];
    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(scripts),
        `Error: The path component for scripts with ` +
            `'match_origin_as_fallback' must be '*'.`);

    chrome.test.succeed();
  },

  // Test that a registered content script will not be injected into a frame
  // where the extension does not have the host permissions for.
  async function noHostPermissions() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'noHostPerms',
      matches: ['*://nohostperms.com/*'],
      js: ['change_title.js'],
      runAt: 'document_end'
    }];

    // check_title.js (manifest content script, for which host permissions do
    // not apply), is run at document_idle. The test passes if the document
    // title sent by check_title.js matches the expected title.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('DOCUMENT_TITLE: OK', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();
    const url = `http://nohostperms.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Test that a registered content script WILL be injected into a frame
  // where the extension has host permissions for.
  async function hostPermissions() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'hostPerms',
      matches: ['*://hostperms.com/*'],
      js: ['change_title.js'],
      runAt: 'document_end'
    }];

    await chrome.scripting.registerContentScripts(scripts);
    const tab = await navigateToRequestedUrl();

    const title = await getTitleForTab(tab.id);
    chrome.test.assertEq('I CHANGED TITLE!!!', title);

    chrome.test.succeed();
  },

  // Test that if the same script file is specified by a manifest content script
  // and a registerContentScripts call, then the script will still only be
  // injected once on a matching frame.
  async function staticAndDynamicScriptInjectedOnce() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end'
    }];

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // matches, but should not be injected.
    const url = `http://b.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);
    const results = await chrome.scripting.executeScript(
        {target: {tabId: tab.id}, func: () => document.body.childElementCount});

    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(1, results[0].result);
    chrome.test.succeed();
  },

  // Test that scripts that are unregistered are not injected into a (former)
  // matching frame.
  async function unregisterScripts() {
    await chrome.scripting.unregisterContentScripts();
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
  // TODO(crbug.com/1300657): This is incorrect, when filter ids is empty it
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
    await chrome.scripting.unregisterContentScripts({ ids: [] });

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

  async function updateScripts() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      excludeMatches: ['*://abc.com/*'],
      js: ['inject_element.js'],
      css: ['nothing.css'],
      runAt: 'document_end',
      allFrames: true
    }];

    var updatedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: ['inject_element_2.js'],
      allFrames: false,
      persistAcrossSessions: false
    }];

    await chrome.scripting.registerContentScripts(scripts);
    let tab = await navigateToRequestedUrl();

    // One element with id 'injected' should be injected.
    chrome.test.assertEq(['injected'], await getInjectedElementIds(tab.id));
    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(1, scripts.length);

    await chrome.scripting.updateContentScripts(updatedScripts);
    tab = await navigateToRequestedUrl();

    // After the script is updated, one element with id 'injected_2' should be
    // injected.
    chrome.test.assertEq(['injected_2'], await getInjectedElementIds(tab.id));

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      excludeMatches: ['*://def.com/*'],
      js: ['inject_element_2.js'],
      css: ['nothing.css'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: false,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if the script ID specified does not
  // match any registered script and that the failed operation does not change
  // the current set of registered scripts.
  async function updateScriptsNonexistentId() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    const nonexistentScriptId = 'NONEXISTENT';
    var updatedScripts = [{
      id: nonexistentScriptId,
      matches: ['*://hostperms.com/*'],
      js: ['inject_element_2.js'],
      runAt: 'document_idle',
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Script with ID '${
            nonexistentScriptId}' does not exist or is not fully registered`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if more than one entry in the API call
  // contains the same script ID and that the failed operation does not change
  // the current set of registered scripts.
  async function updateScriptsDuplicateIdInAPICall() {
    await chrome.scripting.unregisterContentScripts();
    const scriptId = 'inject_element_1';
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    var updatedScripts = [
      {
        id: scriptId,
        matches: ['*://hostperms.com/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_idle',
      },
      {
        id: scriptId,
        matches: ['*://abc.com/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end',
      }
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Duplicate script ID '${scriptId}'`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that updateContentScripts fails if a script is specified with a file
  // that cannot be read.
  async function updateScriptsFileError() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
    }];

    await chrome.scripting.registerContentScripts(scripts);

    const scriptFile = 'NONEXISTENT.js';
    var updatedScripts = [{
      id: 'inject_element_1',
      matches: ['*://hostperms.com/*'],
      js: [scriptFile],
    }];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.updateContentScripts(updatedScripts),
        `Error: Could not load javascript '${scriptFile}' for content script.`);

    const expectedScripts = [{
      id: 'inject_element_1',
      matches: ['*://*/*'],
      js: ['inject_element.js'],
      runAt: 'document_end',
      allFrames: false,
      matchOriginAsFallback: false,
      persistAcrossSessions: true,
      world: chrome.scripting.ExecutionWorld.ISOLATED
    }];

    scripts = await chrome.scripting.getRegisteredContentScripts();
    chrome.test.assertEq(expectedScripts, scripts);

    chrome.test.succeed();
  },

  // Test that if two updateContentScripts calls are made in quick succession,
  // then both calls should succeed in updating their scripts and the old
  // version of these scripts are overwritten.
  // Regression for crbug.com/1454710.
  async function parallelUpdateContentScriptsCalls() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [
      {
        id: 'script_1',
        matches: ['*://*/*'],
        js: ['inject_element.js'],
        runAt: 'document_end',
        allFrames: true
      },
      {
        id: 'script_2',
        matches: ['*://*/*'],
        js: ['inject_element_2.js'],
        runAt: 'document_end',
        allFrames: true
      }
    ];

    // First, register 2 scripts that each inject a different element into the
    // page.
    await chrome.scripting.registerContentScripts(scripts);
    let tab = await navigateToRequestedUrl();

    // Both scripts should be injected, and both scripts should inject one
    // element.
    chrome.test.assertEq(
      ['injected', 'injected_2'], await getInjectedElementIds(tab.id));

    // Now update `script_1` and `script_2` to inject different elements.
    const updatedScript1 = [{
      id: 'script_1',
      matches: ['*://*/*'],
      js: ['inject_element_3.js'],
      allFrames: false,
      persistAcrossSessions: false
    }];

    const updatedScript2 = [{
      id: 'script_2',
      matches: ['*://*/*'],
      js: ['inject_element_4.js'],
      allFrames: true,
      persistAcrossSessions: false
    }];

    await Promise.allSettled([
      chrome.scripting.updateContentScripts(updatedScript1),
      chrome.scripting.updateContentScripts(updatedScript2)
    ]);

    tab = await navigateToRequestedUrl();

    // Check that the old versions of both scripts are not injected by checking
    // that the IDs of the elements injected pertain to the updated scripts.
    chrome.test.assertEq(
      ['injected_3', 'injected_4'], await getInjectedElementIds(tab.id));

    chrome.test.succeed();
  },
]);
