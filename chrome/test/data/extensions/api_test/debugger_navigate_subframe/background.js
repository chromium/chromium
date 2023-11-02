// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';
const DETACHED_WHILE_HANDLING = 'Detached while handling command.';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testNavigateSubframe() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const topURL = config.customArg;
    const subframeURL = topURL.replace('http://a.com', 'http://b.com');
    const tab = await openTab(topURL);
    const debuggee = {tabId: tab.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));

    chrome.debugger.sendCommand(debuggee, 'Page.enable', null);
    const response = await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'Page.getFrameTree', resolve));
    const subframeId = response.frameTree.childFrames[0].frame.id;
    const expression = `
      new Promise(resolve => {
        const frame = document.body.firstElementChild;
        frame.onload = resolve;
        frame.src = '${subframeURL}';
      })
    `;
    await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {
            expression,
            awaitPromise: true
        }, resolve));
    chrome.test.assertNoLastError();
    const result = await new Promise(resolve =>
      chrome.debugger.sendCommand(debuggee, 'Page.navigate', {
          frameId: subframeId,
          url: 'devtools://devtools/bundled/inspector.html'
      }, resolve));
    chrome.test.assertLastError(DETACHED_WHILE_HANDLING);

    chrome.test.succeed();
  }
]));
