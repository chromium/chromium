// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function consoleEventOrdering() {
    const tab = await openTab(chrome.runtime.getURL('inspected.html'));

    const debuggee = {tabId: tab.id};

    const eventOrder = [];

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
          expression: `console.log('Hello World'); 'done'`,
        },
    );

    eventOrder.push(`evaluateResult: ${result.result.value}`);

    await chrome.debugger.detach(debuggee);
    chrome.tabs.remove(tab.id);
    // Check order - console event should come first
    chrome.test.assertEq('consoleEvent: Hello World', eventOrder[0]);
    chrome.test.assertEq('evaluateResult: done', eventOrder[1]);
    chrome.test.succeed();
  },

  async function tracingPermissions() {
    const tab =
        await openTab(`http://b.test:${config.testServer.port}/simple.html`);
    const debuggee = {tabId: tab.id};

    await chrome.debugger.attach(debuggee, protocolVersion);

    let resolveTracingComplete;
    const tracingCompletePromise = new Promise(resolve => {
      resolveTracingComplete = resolve;
    });
    const onEvent = (source, method, params) => {
      if (source.tabId === tab.id && method === 'Tracing.tracingComplete') {
        chrome.debugger.onEvent.removeListener(onEvent);
        resolveTracingComplete(params.stream);
      }
    };
    chrome.debugger.onEvent.addListener(onEvent);

    await chrome.debugger.sendCommand(debuggee, 'Tracing.start', {
      categories: 'blink.user_timing,content,navigation,browser',
      transferMode: 'ReturnAsStream',
    });

    // Global memory dumps are restricted to trusted sessions.
    await chrome.test.assertPromiseRejects(
        chrome.debugger.sendCommand(debuggee, 'Tracing.requestMemoryDump'),
        'Error: {"code":-32000,"message":"Not allowed"}');

    // Emit a trace event in the attached tab's renderer process.
    await chrome.debugger.sendCommand(debuggee, 'Runtime.evaluate', {
      expression: `performance.mark('attached_tab_mark')`,
    });

    // Navigate an unattached tab to trigger browser-process NavigationRequest
    // trace events.
    const secretUrl =
        `http://a.test:${config.testServer.port}/simple.html?secret_token=1`;
    const otherTab = await openTab(secretUrl);

    await chrome.debugger.sendCommand(debuggee, 'Tracing.end');
    const stream = await tracingCompletePromise;

    let traceData = '';
    while (true) {
      const {data, eof} =
          await chrome.debugger.sendCommand(debuggee, 'IO.read', {
            handle: stream,
          });
      traceData += data || '';
      if (eof) {
        break;
      }
    }
    await chrome.debugger.sendCommand(debuggee, 'IO.close', {handle: stream});
    await chrome.debugger.detach(debuggee);
    await chrome.tabs.remove([tab.id, otherTab.id]);

    // Renderer trace events from the attached tab are captured.
    chrome.test.assertTrue(
        traceData.includes('attached_tab_mark'),
        'Expected attached tab renderer trace event to be captured');

    // Browser-process navigation trace events and URLs from other tabs must not
    // be leaked to an untrusted chrome.debugger session.
    chrome.test.assertFalse(
        traceData.includes('secret_token'),
        'Unattached tab navigation URL leaked in trace');
    chrome.test.assertFalse(
        traceData.includes('NavigationRequest'),
        'Browser-process NavigationRequest trace event leaked in trace');

    chrome.test.succeed();
  },
]));
