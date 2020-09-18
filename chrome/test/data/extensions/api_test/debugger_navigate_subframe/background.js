// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';
const DETACHED_WHILE_HANDLING = 'Detached while handling command.';

function openTab(url) {
  return new Promise((resolve) => {
    let createdTabId;
    let completedTabIds = [];
    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
      if (changeInfo.status !== 'complete')
        return;  // Tab not done.

      if (createdTabId === undefined) {
        // A tab completed loading before the chrome.tabs.create callback was
        // triggered; stash the ID for later comparison to see if it was our
        // tab.
        completedTabIds.push(tabId);
        return;
      }

      if (tabId !== createdTabId)
        return;  // Not our tab.

      // It's ours!
      chrome.tabs.onUpdated.removeListener(listener);
      resolve(tab);
    });
    chrome.tabs.create({url: url}, (tab) => {
      if (completedTabIds.includes(tab.id))
        resolve(tab);
      else
        createdTabId = tab.id;
    });
  });
}

chrome.test.getConfig(config => chrome.test.runTests([
  async function testNavigateSubframe() {
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
