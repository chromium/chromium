// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

chrome.test.runTests([async function testCreateTargetToUntrustedWebUIUrl() {
  const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
  const tab = await openTab('about:blank');
  const debuggee = {tabId: tab.id};
  await new Promise(
      resolve => chrome.debugger.attach(debuggee, protocolVersion, resolve));

  await new Promise(
      resolve => chrome.debugger.sendCommand(
          debuggee, 'Target.createTarget', {url: 'chrome-untrusted://terminal'},
          resolve));
  chrome.test.assertLastError(JSON.stringify({
    code: -32000,
    message: 'Refusing to create a target with the specified URL'
  }));

  chrome.test.succeed();
}]);
