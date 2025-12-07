// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kExtensionPath = 'extensions/api_test/webrequest/fencedFrames';

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL(
      `${kExtensionPath}/page_with_network_revoked_fenced_frame.html`, 'a.test',
      'https');
}

function getURLFencedFrame() {
  return getServerURL(
      `${kExtensionPath}/revoke_network.html`, 'a.test', 'https');
}

function getURLFencedFrameNavigation() {
  return getServerURL(`${kExtensionPath}/frame.html`, 'a.test', 'https');
}

runTests([
  // Navigates to a page that a fenced frame. The fenced frame will revoke its
  // own network and then attempt to navigate itself to a new page, which should
  // fail. This tests that the navigation fails and that the events (including
  // the error event) are dispatched correctly.
  function simpleLoadHttp() {
    // MPArch assigns an opaque origin as the initiator for embedder-initiated
    // navigations. Opaque initiators serialize to "null".
    var fencedFrameInitiator = 'null';
    var fencedFrameNavigationInitiator =
        getServerURL('', 'a.test', 'https').slice(0, -1);

    expect(
        [
          // events
          {
            label: 'onBeforeRequest-1',
            event: 'onBeforeRequest',
            details: {
              url: getURLHttpSimpleLoad(),
              frameUrl: getURLHttpSimpleLoad(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onBeforeSendHeaders-1',
            event: 'onBeforeSendHeaders',
            details: {
              url: getURLHttpSimpleLoad(),
              requestHeadersValid: true,
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onSendHeaders-1',
            event: 'onSendHeaders',
            details: {
              url: getURLHttpSimpleLoad(),
              requestHeadersValid: true,
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onHeadersReceived-1',
            event: 'onHeadersReceived',
            details: {
              url: getURLHttpSimpleLoad(),
              responseHeadersExist: true,
              statusLine: 'HTTP/1.1 200 OK',
              statusCode: 200,
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onResponseStarted-1',
            event: 'onResponseStarted',
            details: {
              url: getURLHttpSimpleLoad(),
              statusCode: 200,
              responseHeadersExist: true,
              ip: '127.0.0.1',
              fromCache: false,
              statusLine: 'HTTP/1.1 200 OK',
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onCompleted-1',
            event: 'onCompleted',
            details: {
              url: getURLHttpSimpleLoad(),
              statusCode: 200,
              ip: '127.0.0.1',
              fromCache: false,
              responseHeadersExist: true,
              statusLine: 'HTTP/1.1 200 OK',
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
            }
          },
          {
            label: 'onBeforeRequest-2',
            event: 'onBeforeRequest',
            details: {
              url: getURLFencedFrame(),
              frameUrl: getURLFencedFrame(),
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onBeforeSendHeaders-2',
            event: 'onBeforeSendHeaders',
            details: {
              url: getURLFencedFrame(),
              requestHeadersValid: true,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onSendHeaders-2',
            event: 'onSendHeaders',
            details: {
              url: getURLFencedFrame(),
              requestHeadersValid: true,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onHeadersReceived-2',
            event: 'onHeadersReceived',
            details: {
              url: getURLFencedFrame(),
              responseHeadersExist: true,
              statusLine: 'HTTP/1.1 200 OK',
              statusCode: 200,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onResponseStarted-2',
            event: 'onResponseStarted',
            details: {
              url: getURLFencedFrame(),
              statusCode: 200,
              responseHeadersExist: true,
              ip: '127.0.0.1',
              fromCache: false,
              statusLine: 'HTTP/1.1 200 OK',
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onCompleted-2',
            event: 'onCompleted',
            details: {
              url: getURLFencedFrame(),
              statusCode: 200,
              ip: '127.0.0.1',
              fromCache: false,
              responseHeadersExist: true,
              statusLine: 'HTTP/1.1 200 OK',
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onBeforeRequest-3',
            event: 'onBeforeRequest',
            details: {
              url: getURLFencedFrameNavigation(),
              frameUrl: getURLFencedFrameNavigation(),
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameNavigationInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onBeforeSendHeaders-3',
            event: 'onBeforeSendHeaders',
            details: {
              url: getURLFencedFrameNavigation(),
              requestHeadersValid: true,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameNavigationInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onSendHeaders-3',
            event: 'onSendHeaders',
            details: {
              url: getURLFencedFrameNavigation(),
              requestHeadersValid: true,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameNavigationInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
          {
            label: 'onErrorOccurred-3',
            event: 'onErrorOccurred',
            details: {
              url: getURLFencedFrameNavigation(),
              error: 'net::ERR_NETWORK_ACCESS_REVOKED',
              fromCache: false,
              type: 'sub_frame',
              frameId: 1,
              parentFrameId: 0,
              initiator: fencedFrameNavigationInitiator,
              parentDocumentId: 1,
              frameType: 'fenced_frame'
            }
          },
        ],
        [  // event order
          [
            'onBeforeRequest-1', 'onBeforeSendHeaders-1', 'onSendHeaders-1',
            'onHeadersReceived-1', 'onResponseStarted-1', 'onCompleted-1',
            'onBeforeRequest-2', 'onBeforeSendHeaders-2', 'onSendHeaders-2',
            'onHeadersReceived-2', 'onResponseStarted-2', 'onCompleted-2',
            'onBeforeRequest-3', 'onBeforeSendHeaders-3', 'onSendHeaders-3',
            'onErrorOccurred-3'
          ]
        ],
        {urls: ['<all_urls>']},  // filter
        ['requestHeaders', 'responseHeaders']);
    navigateAndWait(getURLHttpSimpleLoad());
  },
]);
