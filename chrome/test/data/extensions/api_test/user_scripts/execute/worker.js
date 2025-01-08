// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {findDocumentIdWithHostname, findFrameIdWithHostname, getFramesInTab, openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateToRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

async function navigateToNotRequestedUrl() {
  const config = await chrome.test.getConfig();
  const url = `http://not-requested.com:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  // Test that an error is returned if the user script source is not specified.
  async function invalidScriptSource_EmptyJs() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {js: {}, target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if the user script source specifies both
  // code and file.
  async function invalidScriptSource_MultipleSources() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {js: {file: 'script.js', code: ''}, target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user scrip specifies injection to
  // all frames and also a specific set of frame ids.
  async function invalidAllFrames() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {
      js: {file: 'script.js'},
      target: {allFrames: true, frameIds: [456], tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot specify 'allFrames' if either 'frameIds' or ` +
            `'documentIds' is specified.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if the user script has both document ids and
  // frame ids as injection targets.
  async function invalidTarget_DocumentIdAndFrameId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();

    const script = {
      js: {file: 'script.js'},
      target: {documentIds: ['documentId'], frameIds: [456], tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot specify both 'frameIds' and 'documentIds'.`);

    chrome.test.succeed();
  },

  // Tests that an error is thrown when specifying a non-existent document ID.
  async function invalidTarget_documentId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const frames = await getFramesInTab(tab.id);

    const nonExistentDocumentId = '0123456789ABCDEF0123456789ABCDEF';
    const documentIds = [
      findDocumentIdWithHostname(frames, 'requested.com'),
      nonExistentDocumentId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute({
          js: {code: `console.log('hello world')`},
          target: {
            tabId: tab.id,
            documentIds: documentIds,
          }
        }),
        `Error: No document with id ${nonExistentDocumentId} in ` +
            `tab with id ${tab.id}`);

    chrome.test.succeed();
  },

  // Tests that an error is thrown when specifying a non-existent frame ID.
  async function invalidTarget_frameId() {
    await chrome.userScripts.unregister();

    const tab = await navigateToRequestedUrl();
    const frames = await getFramesInTab(tab.id);

    const nonExistentFrameId = 99999;
    const frameIds = [
      findFrameIdWithHostname(frames, 'requested.com'),
      nonExistentFrameId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute({
          js: {code: `console.log('hello world')`},
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          }
        }),
        `Error: No frame with id ${nonExistentFrameId} in ` +
            `tab with id ${tab.id}`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user script has a non-existent tab
  // id.
  async function invalidTarget_tabId() {
    await chrome.userScripts.unregister();

    const tabId = 999;
    const script = {js: {file: 'script.js'}, target: {tabId: tabId}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script), `Error: No tab with id: ${tabId}`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user script doesn't have site
  // access to the page.
  async function invalidTarget_noSiteAccess() {
    await chrome.userScripts.unregister();

    const tab = await navigateToNotRequestedUrl();
    const script = {js: {file: 'script.js'}, target: {tabId: tab.id}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot access contents of the page. Extension manifest must ` +
            `request permission to access the respective host.`);

    chrome.test.succeed();
  },

])
