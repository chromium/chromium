// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testInspectWorkerForbidden() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const tab = await openTab(config.customArg);
    const debuggee = {tabId: tab.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));
    await new Promise(
        resolve => chrome.debugger.sendCommand(
            debuggee, 'ServiceWorker.enable', resolve));
    chrome.test.assertTrue(
        !!chrome.runtime.lastError, `'ServiceWorker.enable' wasn't found`);
    chrome.test.assertEq(
        `'ServiceWorker.enable' wasn't found`,
        JSON.parse(chrome.runtime.lastError.message).message);
    chrome.test.succeed();
  }
]));
