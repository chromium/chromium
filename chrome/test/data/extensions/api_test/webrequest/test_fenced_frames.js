// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kExtensionPath = 'extensions/api_test/webrequest/fencedFrames';

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL(`${kExtensionPath}/main.html`, "a.test", "https");
}

function getURLIntermediateIFrame() {
  return getServerURL(`${kExtensionPath}/iframe.html`, "a.test", "https");
}

function getURLFencedFrame() {
  return getServerURL(`${kExtensionPath}/frame.html`, "a.test", "https");
}

function getURLFencedFrameRedirect() {
  return getServerURL(
      `server-redirect?${kExtensionPath}/frame.html`, "a.test", "https");
}

runTests([
  // Navigates to a page that embeds an iframe that contains a fenced frame.
  // The fenced frame will redirect before landing on the destination fenced
  // frame. This allows us to test that the parentFrameId is the iframe
  // and redirection events are dispatched correctly.
  function simpleLoadHttp() {
    // MPArch assigns an opaque origin as the initiator.
    // Opaque initiators serialize to "null".
    var fencedFrameInitiator = "null";

    expect(
      [
        // events
        { label: 'onBeforeRequest-1',
          event: 'onBeforeRequest',
          details: {
            url: getURLHttpSimpleLoad(),
            frameUrl: getURLHttpSimpleLoad(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: 'onBeforeSendHeaders-1',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLHttpSimpleLoad(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: 'onSendHeaders-1',
          event: 'onSendHeaders',
          details: {
            url: getURLHttpSimpleLoad(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: 'onHeadersReceived-1',
          event: 'onHeadersReceived',
          details: {
            url: getURLHttpSimpleLoad(),
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
            statusCode: 200,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: 'onResponseStarted-1',
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
        { label: 'onCompleted-1',
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
        { label: 'onBeforeRequest-2',
          event: 'onBeforeRequest',
          details: {
            url: getURLIntermediateIFrame(),
            frameUrl: getURLIntermediateIFrame(),
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onBeforeSendHeaders-2',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLIntermediateIFrame(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onSendHeaders-2',
          event: 'onSendHeaders',
          details: {
            url: getURLIntermediateIFrame(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onHeadersReceived-2',
          event: 'onHeadersReceived',
          details: {
            url: getURLIntermediateIFrame(),
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
            statusCode: 200,
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onResponseStarted-2',
          event: 'onResponseStarted',
          details: {
            url: getURLIntermediateIFrame(),
            statusCode: 200,
            responseHeadersExist: true,
            ip: '127.0.0.1',
            fromCache: false,
            statusLine: 'HTTP/1.1 200 OK',
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onCompleted-2',
          event: 'onCompleted',
          details: {
            url: getURLIntermediateIFrame(),
            statusCode: 200,
            ip: '127.0.0.1',
            fromCache: false,
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
            type: 'sub_frame',
            frameId: 1,
            parentFrameId: 0,
            initiator: getServerDomain(initiators.WEB_INITIATED,
              "a.test", "https"),
            parentDocumentId: 1,
            frameType: 'sub_frame'
          }
        },
        { label: 'onBeforeRequest-3',
          event: 'onBeforeRequest',
          details: {
            url: getURLFencedFrameRedirect(),
            frameUrl: getURLFencedFrameRedirect(),
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onBeforeSendHeaders-3',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLFencedFrameRedirect(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onSendHeaders-3',
          event: 'onSendHeaders',
          details: {
            url: getURLFencedFrameRedirect(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onHeadersReceived-3',
          event: 'onHeadersReceived',
          details: {
            url: getURLFencedFrameRedirect(),
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 301 Moved Permanently',
            statusCode: 301,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onBeforeRedirect-3',
          event: 'onBeforeRedirect',
          details: {
            url: getURLFencedFrameRedirect(),
            redirectUrl: getURLFencedFrame(),
            statusCode: 301,
            responseHeadersExist: true,
            ip: '127.0.0.1',
            fromCache: false,
            statusLine: 'HTTP/1.1 301 Moved Permanently',
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onBeforeRequest-4',
          event: 'onBeforeRequest',
          details: {
            url: getURLFencedFrame(),
            frameUrl: getURLFencedFrame(),
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onBeforeSendHeaders-4',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLFencedFrame(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onSendHeaders-4',
          event: 'onSendHeaders',
          details: {
            url: getURLFencedFrame(),
            requestHeadersValid: true,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onHeadersReceived-4',
          event: 'onHeadersReceived',
          details: {
            url: getURLFencedFrame(),
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
            statusCode: 200,
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onResponseStarted-4',
          event: 'onResponseStarted',
          details: {
            url: getURLFencedFrame(),
            statusCode: 200,
            responseHeadersExist: true,
            ip: '127.0.0.1',
            fromCache: false,
            statusLine: 'HTTP/1.1 200 OK',
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
        { label: 'onCompleted-4',
          event: 'onCompleted',
          details: {
            url: getURLFencedFrame(),
            statusCode: 200,
            ip: '127.0.0.1',
            fromCache: false,
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
            type: 'sub_frame',
            frameId: 2,
            parentFrameId: 1,
            initiator: fencedFrameInitiator,
            parentDocumentId: 2,
            frameType: 'fenced_frame'
          }
        },
      ],
      [  // event order
        ['onBeforeRequest-1', 'onBeforeSendHeaders-1', 'onSendHeaders-1',
         'onHeadersReceived-1', 'onResponseStarted-1', 'onCompleted-1',
         'onBeforeRequest-2', 'onBeforeSendHeaders-2',  'onSendHeaders-2',
         'onHeadersReceived-2', 'onResponseStarted-2', 'onCompleted-2',
         'onBeforeRequest-3', 'onBeforeSendHeaders-3',  'onSendHeaders-3',
         'onHeadersReceived-3', 'onBeforeRedirect-3',
         'onBeforeRequest-4', 'onBeforeSendHeaders-4',  'onSendHeaders-4',
         'onHeadersReceived-4', 'onResponseStarted-4', 'onCompleted-4' ] ],
      {urls: ['<all_urls>']},  // filter
      ['requestHeaders', 'responseHeaders']);
    navigateAndWait(getURLHttpSimpleLoad());
  },
]);
