// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testAutoAttachPermissions() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const topURL = config.customArg;
    const subframeURL = topURL.replace('http://a.com', 'http://b.com');
    const pageTab = await openTab(topURL);
    const debuggee = {tabId: pageTab.id};

    const protocolCallbacks = new Map();

    function dispatchMessage(key, args) {
      if (key === 'Target.receivedMessageFromTarget') {
        const message = JSON.parse(args.message);
        if ('id' in message)
          dispatchMessage(message.id, message);
        else
          dispatchMessage(message.method, message.params);
        return;
      }
      const cb = protocolCallbacks.get(key);
      if (!cb)
        return;
      protocolCallbacks.delete(key);
      cb(args);
    }

    chrome.debugger.onEvent.addListener((_, method, params) =>
        dispatchMessage(method, params));

    function onceEvent(name) {
      return new Promise(resolve => {protocolCallbacks.set(name, resolve);});
    }

    let callId = 0;

    function sendMessageToTarget(targetDebuggee, method, params) {
      const message = {
        id: ++callId,
        method,
        params
      };
      chrome.debugger.sendCommand(debuggee, 'Target.sendMessageToTarget', {
        targetId: targetDebuggee.targetId,
        message: JSON.stringify(message)
      });
      return new Promise(resolve => protocolCallbacks.set(message.id, resolve));
    }

    await new Promise(resolve =>
        chrome.debugger.attach(debuggee, protocolVersion, resolve));

    await new Promise(resolve =>
        chrome.debugger.sendCommand(debuggee, 'Target.setAutoAttach', {
            autoAttach: true,
            waitForDebuggerOnStart: false,
            flatten: false},
        resolve));
    chrome.debugger.sendCommand(debuggee, 'Runtime.enable', null);
    const expression = `
        const frame = document.body.firstElementChild;
        frame.src = '${subframeURL}';
    `;
    chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {expression});

    const attachedParams = await onceEvent('Target.attachedToTarget');

    const childDebuggee = {targetId: attachedParams.targetInfo.targetId};
    const anotherTab = await openTab(topURL);
    const targets = await new Promise(resolve =>
        chrome.debugger.getTargets(resolve));

    const anotherTabTarget = targets.find(
        t => t.type === 'page' && t.tabId === anotherTab.id);

    const result = await sendMessageToTarget(childDebuggee,
        'Target.attachToTarget', {targetId: anotherTabTarget.id});

    chrome.test.assertTrue('error' in result, 'Expected error, got success!');
    chrome.test.assertEq(result.error.message, 'Not allowed');

    chrome.test.succeed();
  }
]));
