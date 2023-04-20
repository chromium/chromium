// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const PROTOCOL_VERSION = '1.3';

const HIGH_ENTROPY_API_CATEGORY =
    'disabled-by-default-identifiability.high_entropy_api';
// We will only analyse traces from the HIGH_ENTROPY_API_CATEGORY. While we
// could choose any other category, traces from this one are easy to trigger
// (by calling the respective apis in the html document) and contain a lot of
// information by default (like the execution context). Since they are
// disabled by default we check category enabling on the way.
const includedCategories = [HIGH_ENTROPY_API_CATEGORY];
// Exclude all traces but the ones we explicitly define using the
// includedCategories. Even so, '__metadata' traces will be received, for which
// reason we need to filter the traces later using their category field.
const excludedCategories = ['*'];
const traceConfig = {
  includedCategories,
  excludedCategories,
  // Ensure that we get multiple tracing packages in event based mode that we
  // need to join, in combination with the current number of API calls in the
  // html file. This is just to cover more code paths with this test.
  traceBufferSizeInKb: 100 * 1000,  // 100MB
};
const tracingParams = {traceConfig};

// Waits for tab to be completely loaded. This must be true for the API calls to
// be fetched before tracing is stopped.
function updateTab(tabId, pageUrl) {
  chrome.tabs.update(tabId, {url: pageUrl});

  return new Promise((resolve) => {
    const listener = (_, info, t) => {
      if (t.id === tabId && info.status === 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        resolve();
      }
    };
    chrome.tabs.onUpdated.addListener(listener);
  });
}

// Filters and returns only traces from the HIGH_ENTROPY_API_CATEGORY.
// After we end tracing using sendCommand, dataCollected flow in followed
// by a final tracingComplete event. The number of dataCollected events
// depends on the amount of traced information; since we limit the buffer size
// in the traceConfig we will receive more than one such event (in combination
// with calling enough APIs in the html page).
async function getIdentifiabilityTraces() {
  let traces = [];
  await new Promise((resolve) => {
    const listener = (source, method, params) => {
      if (method === 'Tracing.dataCollected' && params && params.value) {
        traces = traces.concat(params.value.filter(
            (trace) => trace.cat === HIGH_ENTROPY_API_CATEGORY &&
                       trace.name === "HighEntropyJavaScriptAPICall"));
      } else if (method === 'Tracing.tracingComplete') {
        chrome.debugger.onEvent.removeListener(listener);
        resolve();
      }
    };
    chrome.debugger.onEvent.addListener(listener);
  });
  return traces;
}

chrome.test.getConfig((config) => {
  const crossOriginA = `http://a.test:${config.testServer.port}`;
  const crossOriginB = `http://b.test:${config.testServer.port}`;

  const filterOriginA = (traces) => {
    return traces.filter(
        (trace) => trace.args.high_entropy_api.execution_context.origin ===
            crossOriginA);
  };
  const filterOriginB = (traces) => {
    return traces.filter(
        (trace) => trace.args.high_entropy_api.execution_context.origin ===
            crossOriginB);
  };

  const htmlPath =
      '/extensions/api_test/tracing_extension/test_api_tracing.html';
  const pageA = `${crossOriginA}${htmlPath}`;
  const pageB = `${crossOriginB}${htmlPath}`;

  // Number of API calls triggered by loading the html file one time.
  // Depends on the number of api calls emitted by the inline script in the html
  // file and the number of high entropy traces emitted for each such call.
  const API_CALL_COUNT = 62;

  chrome.test.runTests([
    // The current tab's traces should be receiveable using the default tracing
    // filter, which filters the current tab's processes ('currentTarget').
    async function traceTargetTabEventBased() {
      const targetTab = await chrome.tabs.create({url: 'about:blank'});
      const debuggee = {tabId: targetTab.id};

      await chrome.debugger.attach(debuggee, PROTOCOL_VERSION);

      await chrome.debugger.sendCommand(
          debuggee, 'Tracing.start', tracingParams);

      // API calls are triggered using reload events on the current target.
      await updateTab(targetTab.id, pageB);
      await updateTab(targetTab.id, pageA);

      // Traces are received using events after the end command is send.
      chrome.debugger.sendCommand(debuggee, 'Tracing.end');
      const traces = await getIdentifiabilityTraces();

      // Check if traces can be received from different origins and are
      // attributed to them.
      chrome.test.assertEq(API_CALL_COUNT, filterOriginA(traces).length);
      chrome.test.assertEq(API_CALL_COUNT, filterOriginB(traces).length);

      chrome.debugger.detach(debuggee);
      await chrome.tabs.remove(targetTab.id);
      chrome.test.succeed();
    },

    // Only traces from the target tab should be received.
    async function notTraceOtherTabEventBased() {
      const targetTab = await chrome.tabs.create({url: 'about:blank'});
      const otherTab = await chrome.tabs.create({url: 'about:blank'});
      const debuggee = {tabId: targetTab.id};

      await chrome.debugger.attach(debuggee, PROTOCOL_VERSION);

      await chrome.debugger.sendCommand(
          debuggee, 'Tracing.start', tracingParams);

      // API calls are triggered using reload events on the tabs.
      await updateTab(otherTab.id, pageA);
      await updateTab(targetTab.id, pageB);

      chrome.debugger.sendCommand(debuggee, 'Tracing.end');
      const traces = await getIdentifiabilityTraces();

      // All the API calls triggered in the other tab should be ignored.
      chrome.test.assertEq(0, filterOriginA(traces).length);
      // Only receive the target tabs traces.
      chrome.test.assertEq(API_CALL_COUNT, filterOriginB(traces).length);

      chrome.debugger.detach(debuggee);

      await chrome.tabs.remove(targetTab.id);
      await chrome.tabs.remove(otherTab.id);
      chrome.test.succeed();
    },
  ]);
});
