// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inServiceWorker = 'ServiceWorkerGlobalScope' in self;
const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let ready;
const onScriptLoad = chrome.test.loadScript(scriptUrl);

if (inServiceWorker) {
  ready = onScriptLoad;
} else {
  let onWindowLoad = new Promise((resolve) => {
    window.onload = resolve;
  });
  ready = Promise.all([onWindowLoad, onScriptLoad]);
}

ready.then(async function() {
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;

  // This test verifies the order of events of the first prerendering frame
  // and the prerender activation. The order of the first prerendering
  // navigation is onBeforeNavigate => onCommitted, and the one of the
  // prerendering activation is onBeforeNavigate => onCommitted =>
  // onDOMContentLoaded => onCompleted. DOMContentLoaded is dipatched on
  // activation, because this is to avoid notifying observers about a load event
  // triggered from an inactive RenderFrameHost.
  chrome.test.runTests([
    async function testVerifyPrerenderingFramesCallbackOrder() {
      const urlPrefix =
          `http://a.test:${port}/extensions/api_test/webnavigation/prerendering/`;
      const prerenderTargetUrl = urlPrefix + 'a.html';
      const initiatorUrl = urlPrefix + 'prerender.html';
      let expectedEvents = [
        // events
        {
          label: 'onBeforeNavigate-1',
          event: 'onBeforeNavigate',
          details: {
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: -1,
            tabId: 0,
            timeStamp: 0,
            url: initiatorUrl
          }
        },
        {
          label: 'onCommitted-1',
          event: 'onCommitted',
          details: {
            documentId: 1,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 0,
            tabId: 0,
            timeStamp: 0,
            transitionQualifiers:[],
            transitionType:"link",
            url: initiatorUrl
          }
        },
        {
          label: 'onDOMContentLoaded-1',
          event: 'onDOMContentLoaded',
          details: {
            documentId: 1,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 0,
            tabId: 0,
            timeStamp: 0,
            url: initiatorUrl
          }
        },
        {
          label: 'onCompleted-1',
          event: 'onCompleted',
          details: {
            documentId: 1,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 0,
            tabId: 0,
            timeStamp: 0,
            url: initiatorUrl
          }
        },
        {
          label: 'onBeforeNavigate-2',
          event: 'onBeforeNavigate',
          details: {
            documentLifecycle: 'prerender',
            frameId: 1,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: -1,
            tabId: 0,
            timeStamp: 0,
            url: prerenderTargetUrl
          }
        },
        {
          label: 'onCommitted-2',
          event: 'onCommitted',
          details: {
            documentId: 2,
            documentLifecycle: 'prerender',
            frameId: 1,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 1,
            tabId: 0,
            timeStamp: 0,
            transitionQualifiers:[],
            transitionType:"link",
            url: prerenderTargetUrl
          }
        },
        {
          label: 'onBeforeNavigate-3',
          event: 'onBeforeNavigate',
          details: {
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: -1,
            tabId: 0,
            timeStamp: 0,
            url: prerenderTargetUrl
          }
        },
        {
          label: 'onCommitted-3',
          event: 'onCommitted',
          details: {
            documentId: 2,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 1,
            tabId: 0,
            timeStamp: 0,
            transitionQualifiers:[],
            transitionType:"link",
            url: prerenderTargetUrl
          }
        },
        {
          label: 'onDOMContentLoaded-3',
          event: 'onDOMContentLoaded',
          details: {
            documentId: 2,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 1,
            tabId: 0,
            timeStamp: 0,
            url: prerenderTargetUrl
          }
        },
        {
          label: 'onCompleted-3',
          event: 'onCompleted',
          details: {
            documentId: 2,
            documentLifecycle: 'active',
            frameId: 0,
            frameType: 'outermost_frame',
            parentFrameId: -1,
            processId: 1,
            tabId: 0,
            timeStamp: 0,
            url: prerenderTargetUrl
          }
        },
      ];

      let expectedPrerenderedOrder = ['onBeforeNavigate-2', 'onCommitted-2'];

      if (!inServiceWorker) {
        expectedEvents.push(
          // TODO(crbug.com/40365717): Remove this expectation when the crbug
          // is fixed.
          {
            label: 'onCommitted-2-activation-callback',
            event: 'onCommitted',
            details: {
              documentId: 2,
              documentLifecycle: 'prerender',
              frameId: 1,
              frameType: 'outermost_frame',
              parentFrameId: -1,
              processId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers:[],
              transitionType:"link",
              url: prerenderTargetUrl
            }
          });
          expectedPrerenderedOrder.push('onCommitted-2-activation-callback');
      }

      expect(
          expectedEvents,
          [
            // Events
            // *-1: for navigate to the initiator page.
            // *-2: for prerendering.
            // *-3: for prerendering activation.
            ['onBeforeNavigate-1', 'onCommitted-1',
            'onDOMContentLoaded-1', 'onCompleted-1'],
            expectedPrerenderedOrder,
            ['onBeforeNavigate-3', 'onCommitted-3',
            'onDOMContentLoaded-3', 'onCompleted-3'],
            ['onBeforeNavigate-1', 'onBeforeNavigate-2', 'onBeforeNavigate-3']
          ],
          { urls: ['<all_urls>'] },  // filter
          []);

      const activationCallback = details => {
        chrome.test.assertEq('prerender', details.documentLifecycle);
        chrome.tabs.executeScript({
          code: `location.href = '${prerenderTargetUrl}';`,
          runAt: 'document_idle'
        });
        chrome.webNavigation.onCommitted.removeListener(activationCallback);
      };

      chrome.webNavigation.onCommitted.addListener(
        activationCallback, {url: [{pathContains: '/a.html'}]});

      // Navigate to a page that initiates prerendering "a.html".
      let tab = await promise(chrome.tabs.create, {"url": initiatorUrl});
    },
  ]);
});
