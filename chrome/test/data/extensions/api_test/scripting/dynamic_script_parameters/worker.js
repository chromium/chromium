// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.runTests([
  async function paramsAvailableForContentScripts() {
    // The params are not accessible within a service worker context.
    chrome.test.assertEq(undefined, chrome.scripting.globalParams);

    await chrome.scripting.unregisterContentScripts();
    var scripts = [{
      id: 'script1',
      matches: ['*://a.com/*'],
      js: ['check_params.js'],
      runAt: 'document_end'
    }];

    // Verify that the default params value seen by the content script is an
    // empty JS object.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('GLOBALPARAMS: {}', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // will be injected.
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Test 2 scripts injected at different times. First script changes the
  // parameters' value, which should be seen by the second script.
  async function paramsModifiedByScripts() {
    await chrome.scripting.unregisterContentScripts();
    var scripts = [
      {
        id: 'script1',
        matches: ['*://a.com/*'],
        js: ['change_params.js'],
        runAt: 'document_end'
      },
      {
        id: 'script2',
        matches: ['*://a.com/*'],
        js: ['check_params.js'],
        runAt: 'document_idle'
      }
    ];

    // Verify that the params object seen by `script2` contains the field added
    // by `script1`.
    chrome.runtime.onMessage.addListener(function passTest(
        message, sender, sendResponse) {
      chrome.runtime.onMessage.removeListener(passTest);
      chrome.test.assertEq('GLOBALPARAMS: {"value":"changed"}', message);
      chrome.test.succeed();
    });

    await chrome.scripting.registerContentScripts(scripts);
    const config = await chrome.test.getConfig();

    // After the script has been registered, Navigate to a url where the script
    // will be injected.
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    chrome.tabs.create({url});
  },

  // Test that parameters are only accessible to scripts/functions injected in
  // an isolated world, and not the main world.
  async function paramsNotVisibleInMainWorld() {
    await chrome.scripting.unregisterContentScripts();
    const config = await chrome.test.getConfig();
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    const tab = await openTab(url);

    const checkParams = () => !!chrome.scripting ?
        JSON.stringify(chrome.scripting.globalParams) :
        'undefined';

    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      world: 'ISOLATED',
      func: checkParams,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('{}', results[0].result);

    results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      world: 'MAIN',
      func: checkParams,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('undefined', results[0].result);

    chrome.test.succeed();
  },

  // Test that changes to parameters' values are applied only in one context and
  // will not propagate to other contexts/tabs.
  async function paramsIsolatedBetweenContexts() {
    await chrome.scripting.unregisterContentScripts();
    const config = await chrome.test.getConfig();
    const url = `http://a.com:${config.testServer.port}/simple.html`;
    const firstTab = await openTab(url);
    const secondTab = await openTab(url);

    const checkParams = () => JSON.stringify(chrome.scripting.globalParams);
    const setParams = () => {
      chrome.scripting.globalParams.value = 'set';
    };

    await chrome.scripting.executeScript({
      target: {
        tabId: firstTab.id,
      },
      func: setParams,
    });

    let results = await chrome.scripting.executeScript({
      target: {
        tabId: firstTab.id,
      },
      func: checkParams,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('{"value":"set"}', results[0].result);

    results = await chrome.scripting.executeScript({
      target: {
        tabId: secondTab.id,
      },
      func: checkParams,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('{}', results[0].result);

    chrome.test.succeed();
  },
]);
