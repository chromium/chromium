// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

import {openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testOopifAutoAttachWarBypass() {
    const args = config.customArg.split(';');
    const topURL = args[0];
    const victimId = args[1];

    // b.com is used to make the iframe an OOPIF.
    const subframeURL =
        topURL.replace('a.com', 'b.com').replace('page.html', 'empty.html');

    const pageTab = await openTab(topURL);
    const debuggee = {tabId: pageTab.id};

    const eventCallbacks = new Map();

    chrome.debugger.onEvent.addListener((source, method, params) => {
      const cb = eventCallbacks.get(method);
      if (!cb) {
        return;
      }
      eventCallbacks.delete(method);
      cb({source, params});
    });

    function onceEvent(name) {
      return new Promise(resolve => {
        eventCallbacks.set(name, resolve);
      });
    }

    await chrome.debugger.attach(debuggee, protocolVersion);

    await chrome.debugger.sendCommand(
        debuggee, 'Target.setAutoAttach',
        {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
    await chrome.debugger.sendCommand(debuggee, 'Runtime.enable', null);

    const expression = `
        const frame = document.body.firstElementChild;
        frame.src = '${subframeURL}';
    `;
    // We expect an OOPIF to be created and auto-attached.
    const attachedPromise = onceEvent('Target.attachedToTarget');
    chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {expression});

    const {params: attachedParams} = await attachedPromise;
    const childDebuggerSession = {
      ...debuggee,
      sessionId: attachedParams.sessionId,
    };

    const restrictedURL = `chrome-extension://${victimId}/restricted.html`;

    // Wait for the navigation to finish inside the child session.
    // If it is blocked, errorText will contain 'net::ERR_BLOCKED_BY_CLIENT'.
    const navigateResult = await chrome.debugger.sendCommand(
        childDebuggerSession, 'Page.navigate', {url: restrictedURL});

    // Assert that the request was blocked by the browser.
    chrome.test.assertTrue(!!navigateResult.errorText);
    chrome.test.assertTrue(
        navigateResult.errorText.includes('ERR_BLOCKED_BY_CLIENT'));

    chrome.test.succeed();
  },
]));
