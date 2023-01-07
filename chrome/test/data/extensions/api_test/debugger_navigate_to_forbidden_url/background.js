// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';
const DETACHED_WHILE_HANDLING = 'Detached while handling command.';

chrome.test.runTests([
  async function testNavigateToForbiddenUrl() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const tab = await openTab('about:blank');
    const debuggee = {tabId: tab.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));
    chrome.debugger.sendCommand(debuggee, 'Page.crash');
    await new Promise(resolve =>
        chrome.debugger.onEvent.addListener((source, method, params) => {
          if (method === 'Inspector.targetCrashed')
            resolve();
        }));
    const result = await new Promise(resolve =>
      chrome.debugger.sendCommand(debuggee, 'Page.navigate', {
          url: 'chrome://version'
      }, resolve));
    chrome.test.assertLastError(DETACHED_WHILE_HANDLING);
    chrome.test.succeed();
  }
]);
