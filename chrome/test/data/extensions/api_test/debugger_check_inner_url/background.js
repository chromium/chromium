// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';
const DETACHED_WHILE_HANDLING = 'Detached while handling command.';

async function findTarget(url) {
  const targets = await new Promise(resolve =>
      chrome.debugger.getTargets(resolve));
  return targets.find(target => url === target.url);
}

chrome.test.getConfig(config => chrome.test.runTests([
  async function testAccessToNavigationTarget() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const pagePath = 'extensions/api_test/debugger_check_inner_url/page.html';
    const topURL = `http://a.com:${config.testServer.port}/${pagePath}`;
    const subframeURL = `http://b.com:${config.testServer.port}/${pagePath}`;
    const tab = await openTab(topURL);
    const debuggee = {tabId: tab.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));

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

    const subframeTarget = await findTarget(subframeURL);
    const subframeDebuggee = {targetId: subframeTarget.id};
    await new Promise(resolve =>
        chrome.debugger.attach(subframeDebuggee, protocolVersion, resolve));

    await new Promise(resolve =>
        chrome.debugger.sendCommand(subframeDebuggee, 'Page.navigate', {
            url: 'blob:chrome://non-existent/'}, resolve));

    chrome.test.assertLastError(DETACHED_WHILE_HANDLING);

    chrome.test.succeed();
  }
]));
