// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';
const DETACHED_WHILE_HANDLING = 'Detached while handling command.';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testNavigateSubframe() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const topURL = config.customArg;
    const subframeURL = topURL.replace('http://a.com', 'http://b.com');

    let requestExtraInfoCount = 0;
    let responseExtraInfoCount = 0;
    function onEvent(debuggeeId, message, params) {
      if (message === 'Network.requestWillBeSentExtraInfo') {
        requestExtraInfoCount++;
      } else if (message ===  'Network.responseReceivedExtraInfo') {
        responseExtraInfoCount++;
      }
    }
    chrome.debugger.onEvent.addListener(onEvent);

    const tab = await openTab(topURL);
    const debuggee = {tabId: tab.id};
    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));
    chrome.debugger.sendCommand(debuggee, 'Page.enable', null);
    chrome.debugger.sendCommand(debuggee, 'Network.enable', null);
    await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'Page.getFrameTree', resolve));

    // Navigation causes OOPIF-transfer
    // Verify that ExtraInfo events are being received
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
    chrome.test.assertTrue(requestExtraInfoCount > 0,
        'No "requestWillBeSentExtraInfo" event received for OOPIF transfer');
    requestExtraInfoCount = 0;
    chrome.test.assertTrue(responseExtraInfoCount > 0,
        'No "responseReceivedExtraInfo" event received for OOPIF transfer');
    responseExtraInfoCount = 0;

    // Attach to OOPIF target
    const targets = await new Promise(resolve =>
      chrome.debugger.getTargets(resolve));
    const oopifTarget = targets.find(target => target.url === subframeURL);
    const frameTarget = {targetId: oopifTarget.id};
    await new Promise(resolve =>
        chrome.debugger.attach(frameTarget, protocolVersion, resolve));
    chrome.debugger.sendCommand(frameTarget, 'Network.enable', null);

    // Navigation to restricted origin
    // Verify that no ExtraInfo events are being received
    const restrictedURL = topURL.replace('http://a.com', 'http://c.com');
    const expression2 = `
      new Promise(resolve => {
        const frame = document.body.firstElementChild;
        frame.onload = resolve;
        frame.src = '${restrictedURL}';
      })
    `;
    await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {
            expression: expression2,
            awaitPromise: true
        }, resolve));

    chrome.test.assertLastError(DETACHED_WHILE_HANDLING);
    chrome.test.assertEq(0, requestExtraInfoCount,
        '"requestWillBeSentExtraInfo" event received for navigation to ' +
        '"restricted origin');
    chrome.test.assertEq(0, responseExtraInfoCount,
      '"responseReceivedExtraInfo" event received for navigation to ' +
      'restricted origin');
    chrome.test.succeed();
  }
]));
