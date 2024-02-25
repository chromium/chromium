// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kExtensionPath = 'extensions/api_test/webrequest/prerendering';

runTests([function simpleLoad() {
  const kInitiatorUrl =
      getServerURL(`${kExtensionPath}/initiator_new_tab.html`);
  const kPrerenderingUrl =
      getServerURL(`${kExtensionPath}/prerendering_new_tab.html`);
  const kEmptyJsUrl = getServerURL(`${kExtensionPath}/empty.js`);

  expect(
      [
        // events
        {
          label: 'onBeforeRequest-1',
          event: 'onBeforeRequest',
          details: {
            url: kInitiatorUrl,
            frameUrl: kInitiatorUrl,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active'
          }
        },
        {
          label: 'onBeforeSendHeaders-1',
          event: 'onBeforeSendHeaders',
          details: {
            url: kInitiatorUrl,
            requestHeadersValid: true,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active'
          }
        },
        {
          label: 'onSendHeaders-1',
          event: 'onSendHeaders',
          details: {
            url: kInitiatorUrl,
            requestHeadersValid: true,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active'
          }
        },
        {
          label: 'onHeadersReceived-1',
          event: 'onHeadersReceived',
          details: {
            url: kInitiatorUrl,
            responseHeadersExist: true,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK'
          }
        },
        {
          label: 'onResponseStarted-1',
          event: 'onResponseStarted',
          details: {
            url: kInitiatorUrl,
            responseHeadersExist: true,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1'
          }
        },
        {
          label: 'onCompleted-1',
          event: 'onCompleted',
          details: {
            url: kInitiatorUrl,
            responseHeadersExist: true,
            method: 'GET',
            frameId: 0,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'active',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1'
          }
        },
        {
          label: 'onBeforeRequest-2',
          event: 'onBeforeRequest',
          details: {
            url: kPrerenderingUrl,
            frameUrl: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onBeforeSendHeaders-2',
          event: 'onBeforeSendHeaders',
          details: {
            url: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            requestHeadersValid: true,
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onSendHeaders-2',
          event: 'onSendHeaders',
          details: {
            url: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            requestHeadersValid: true,
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onHeadersReceived-2',
          event: 'onHeadersReceived',
          details: {
            url: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            tabId: 1
          }
        },
        {
          label: 'onResponseStarted-2',
          event: 'onResponseStarted',
          details: {
            url: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1',
            tabId: 1
          }
        },
        {
          label: 'onCompleted-2',
          event: 'onCompleted',
          details: {
            url: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'main_frame',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1',
            tabId: 1
          }
        },
        {
          label: 'onBeforeRequest-3',
          event: 'onBeforeRequest',
          details: {
            url: kEmptyJsUrl,
            frameUrl: kPrerenderingUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onBeforeSendHeaders-3',
          event: 'onBeforeSendHeaders',
          details: {
            url: kEmptyJsUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            requestHeadersValid: true,
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onSendHeaders-3',
          event: 'onSendHeaders',
          details: {
            url: kEmptyJsUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            requestHeadersValid: true,
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            tabId: 1
          }
        },
        {
          label: 'onHeadersReceived-3',
          event: 'onHeadersReceived',
          details: {
            url: kEmptyJsUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            tabId: 1
          }
        },
        {
          label: 'onResponseStarted-3',
          event: 'onResponseStarted',
          details: {
            url: kEmptyJsUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1',
            tabId: 1
          }
        },
        {
          label: 'onCompleted-3',
          event: 'onCompleted',
          details: {
            url: kEmptyJsUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            responseHeadersExist: true,
            method: 'GET',
            frameId: 1,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'script',
            documentLifecycle: 'prerender',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            fromCache: false,
            ip: '127.0.0.1',
            tabId: 1
          }
        },
        {
          label: 'onBeforeRequest-4',
          event: 'onBeforeRequest',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            frameUrl: 'unknown frame URL',
            tabId: 1
          },
        },
        {
          label: 'onBeforeSendHeaders-4',
          event: 'onBeforeSendHeaders',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            requestHeadersValid: true,
            tabId: 1
          },
        },
        {
          label: 'onSendHeaders-4',
          event: 'onSendHeaders',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            requestHeadersValid: true,
            tabId: 1
          },
        },
        {
          label: 'onHeadersReceived-4',
          event: 'onHeadersReceived',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            responseHeadersExist: true,
            tabId: 1
          },
        },
        {
          label: 'onResponseStarted-4',
          event: 'onResponseStarted',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            responseHeadersExist: true,
            fromCache: false,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            tabId: 1
          },
        },
        {
          label: 'onCompleted-4',
          event: 'onCompleted',
          details: {
            url: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active',
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            responseHeadersExist: true,
            fromCache: false,
            tabId: 1
          },
        }
      ],
      [
        // Events
        // *-1: for navigate to the initiator page.
        // *-2: for prerendering.
        // *-3: for a script sub-resource in the prerendering page.
        // *-4: for fetch request made after the page activation.
        ['onBeforeRequest-1', 'onBeforeSendHeaders-1', 'onSendHeaders-1',
          'onHeadersReceived-1', 'onResponseStarted-1', 'onCompleted-1'],
          ['onBeforeRequest-2', 'onBeforeSendHeaders-2', 'onSendHeaders-2',
          'onHeadersReceived-2', 'onResponseStarted-2', 'onCompleted-2'],
          ['onBeforeRequest-3', 'onBeforeSendHeaders-3', 'onSendHeaders-3',
          'onHeadersReceived-3', 'onResponseStarted-3', 'onCompleted-3'],
          ['onBeforeRequest-4', 'onBeforeSendHeaders-4', 'onSendHeaders-4',
           'onHeadersReceived-4', 'onResponseStarted-4', 'onCompleted-4'],
      ],
      {urls: ['<all_urls>']},  // filter
      ['requestHeaders', 'responseHeaders']);
  // Set another listener to know the pre-rendered page is ready and to
  // activate the page.
  let initiatorTabId = -1;
  const activationCallback = details => {
    if (details.url === kInitiatorUrl) {
      initiatorTabId = details.tabId;
    }

    if (details.url === kEmptyJsUrl) {
      chrome.tabs.executeScript(initiatorTabId, {
        code: `document.getElementById(\'link\').click();`,
        runAt: 'document_idle'
      });
      chrome.webRequest.onCompleted.removeListener(activationCallback);
    }
  };
  chrome.webRequest.onCompleted.addListener(
      activationCallback, {urls: [kInitiatorUrl, kEmptyJsUrl]}, []);
  navigateAndWait(kInitiatorUrl);
}]);
