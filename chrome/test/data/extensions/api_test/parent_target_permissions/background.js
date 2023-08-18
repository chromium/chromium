// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testParentTargetPermissions() {
    const fileFrameURL =
        config.testDataDirectory + '/parent_target_permissions/top_page.html';
    const subframeURL = chrome.runtime.getURL('subframe.html');
    chrome.test.openFileUrl(fileFrameURL + '?' + subframeURL);
    await new Promise(resolve => {
      chrome.runtime.onMessage.addListener(message => {
        if (message === 'ready')
          resolve();
      });
    });
    const targets = await new Promise(resolve =>
        chrome.debugger.getTargets(resolve));
    const subframeTarget =
        targets.find(t => t.type === 'other' && t.url === subframeURL);
    const debuggee = {targetId: subframeTarget.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));
    chrome.test.assertLastError('Cannot attach to this target.');
    chrome.test.succeed();
  }
]));
