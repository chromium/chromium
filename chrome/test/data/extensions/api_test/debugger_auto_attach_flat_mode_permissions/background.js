// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

import {openTab} from '/_test_resources/test_util/tabs_util.js';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testAutoAttachFlatModePermissions() {
    const topURL = config.customArg;
    const subframeURL = topURL.replace('http://a.com', 'http://b.com');
    const pageTab = await openTab(topURL);
    const debuggee = {tabId: pageTab.id};

    const eventCallbacks = new Map();

    chrome.debugger.onEvent.addListener((debuggee, method, params) => {
      const cb = eventCallbacks.get(method);
      if (!cb)
        return;
      eventCallbacks.delete(method);
      cb({debuggee, params});
    });

    function onceEvent(name) {
      return new Promise(resolve => {
        eventCallbacks.set(name, resolve);
      });
    }

    await chrome.debugger.attach(debuggee, protocolVersion);

    await chrome.debugger.sendCommand(debuggee, 'Target.setAutoAttach', {
      autoAttach: true,
      waitForDebuggerOnStart: false,
      flatten: true});
    await chrome.debugger.sendCommand(debuggee, 'Runtime.enable', null);
    const expression = `
        const frame = document.body.firstElementChild;
        frame.src = '${subframeURL}';
    `;
    chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {expression});

    const {params: attachedParams} = await onceEvent('Target.attachedToTarget');
    const childDebuggerSession = {
      ...debuggee,
      sessionId: attachedParams.sessionId
    };
    const anotherTab = await openTab(topURL);
    const targets = await new Promise(resolve =>
        chrome.debugger.getTargets(resolve));

    const anotherTabTarget = targets.find(
        t => t.type === 'page' && t.tabId === anotherTab.id);

    // Child session does not allow connecting to another target.
    await chrome.test.assertPromiseRejects(chrome.debugger.sendCommand(
      childDebuggerSession,
      'Target.attachToTarget',
      {targetId: anotherTabTarget.id},
    ), 'Error: {"code":-32000,"message":"Not allowed"}');

    // Can evaluate in the child session.
    const response = await chrome.debugger.sendCommand(
      childDebuggerSession,
      'Runtime.evaluate',
      {expression: 'window.location.href'},
    );
    chrome.test.assertEq('b.com', new URL(response.result.value).hostname);


    await chrome.debugger.sendCommand(
      childDebuggerSession,
      'Runtime.enable',
      null,
    );

    // Can receive events from the child session.
    const eventPromise = onceEvent('Runtime.consoleAPICalled')
    await chrome.debugger.sendCommand(
      childDebuggerSession,
      'Runtime.evaluate',
      {expression: 'console.log("test")'},
    );
    const message = await eventPromise;
    chrome.test.assertEq('test', message.params.args[0].value);
    chrome.test.assertEq(attachedParams.sessionId, message.debuggee.sessionId);

    chrome.test.succeed();
  }
]));
