// Copyright 2020 The Chromium Authors. All rights reserved.
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
    chrome.debugger.sendCommand(debuggee, 'ServiceWorker.enable', null);
    let workerReadyCallback;
    chrome.debugger.onEvent.addListener((source, method, params) => {
      if (method !== 'ServiceWorker.workerVersionUpdated')
        return;
      const versions = params.versions;
      if (!versions.length || versions[0].runningStatus !== 'running')
        return;
      workerReadyCallback(versions[0].versionId);
    });
    const versionId = await new Promise(resolve =>
        workerReadyCallback = resolve);
    await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'ServiceWorker.inspectWorker',
            {versionId}, resolve))
    chrome.test.assertTrue(!!chrome.runtime.lastError,
                           'Expected ServiceWorker.inspectWorker to fail');
    chrome.test.assertEq('Permission denied',
                         JSON.parse(chrome.runtime.lastError.message).message);
    chrome.test.succeed();
  }
]));
