// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { openTab } from '/_test_resources/test_util/tabs_util.js';

var protocolVersion = "1.3";

chrome.test.getConfig(config => chrome.test.runTests([
  async function consoleEventOrdering() {
    const tab = await openTab(chrome.runtime.getURL('inspected.html'));

    const debuggee = { tabId: tab.id };

    let eventOrder = [];

    await chrome.debugger.attach(debuggee, protocolVersion);

    await chrome.debugger.sendCommand(debuggee, 'Runtime.enable');

    chrome.debugger.onEvent.addListener((source, method, params) => {
      if (method === 'Runtime.consoleAPICalled') {
        const args = params.args.map(arg => arg.value).join(', ');
        eventOrder.push(`consoleEvent: ${args}`);
      }
    });

    const result = await chrome.debugger.sendCommand(
      debuggee,
      'Runtime.evaluate',
      {
        expression: 'console.log("Hello World"); "done"',
      }
    );

    eventOrder.push(`evaluateResult: ${result.result.value}`);

    await chrome.debugger.detach(debuggee);
    chrome.tabs.remove(tab.id);
    // Check order - console event should come first
    chrome.test.assertEq('consoleEvent: Hello World', eventOrder[0]);
    chrome.test.assertEq('evaluateResult: done', eventOrder[1]);
    chrome.test.succeed();
  },
]));
