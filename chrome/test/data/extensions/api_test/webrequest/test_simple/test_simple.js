// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL('extensions/api_test/webrequest/simpleLoad/a.html');
}

function getURLHttpSimpleLoadRedirect() {
  return getServerURL('server-redirect?'+getURLHttpSimpleLoad());
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Navigates to a blank page.
  function simpleLoad() {
    expect(
      [  // events
        { label: "a-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURL("a.html"),
            frameUrl: getURL("a.html"),
            initiator: getDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "a-onResponseStarted",
          event: "onResponseStarted",
          details: {
            url: getURL("a.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getDomain(initiators.BROWSER_INITIATED),
            // Request to chrome-extension:// url has no IP.
          }
        },
        { label: "a-onCompleted",
          event: "onCompleted",
          details: {
            url: getURL("a.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getDomain(initiators.BROWSER_INITIATED),
            // Request to chrome-extension:// url has no IP.
          }
        },
      ],
      [  // event order
      ["a-onBeforeRequest", "a-onResponseStarted", "a-onCompleted"] ]);
    navigateAndWait(getURL("a.html"));
  },

  // Navigates to a blank page via HTTP. Only HTTP requests get the
  // onBeforeSendHeaders event.
  function simpleLoadHttp() {
    expect(
      [  // events
        { label: "onBeforeRequest-1",
          event: "onBeforeRequest",
          details: {
            url: getURLHttpSimpleLoadRedirect(),
            frameUrl: getURLHttpSimpleLoadRedirect(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeSendHeaders-1",
          event: "onBeforeSendHeaders",
          details: {
            url: getURLHttpSimpleLoadRedirect(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onSendHeaders-1",
          event: "onSendHeaders",
          details: {
            url: getURLHttpSimpleLoadRedirect(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onHeadersReceived-1",
          event: "onHeadersReceived",
          details: {
            url: getURLHttpSimpleLoadRedirect(),
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 301 Moved Permanently",
            statusCode: 301,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeRedirect",
          event: "onBeforeRedirect",
          details: {
            url: getURLHttpSimpleLoadRedirect(),
            redirectUrl: getURLHttpSimpleLoad(),
            statusCode: 301,
            responseHeadersExist: true,
            ip: "127.0.0.1",
            fromCache: false,
            statusLine: "HTTP/1.1 301 Moved Permanently",
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeRequest-2",
          event: "onBeforeRequest",
          details: {
            url: getURLHttpSimpleLoad(),
            frameUrl: getURLHttpSimpleLoad(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeSendHeaders-2",
          event: "onBeforeSendHeaders",
          details: {
            url: getURLHttpSimpleLoad(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onSendHeaders-2",
          event: "onSendHeaders",
          details: {
            url: getURLHttpSimpleLoad(),
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onHeadersReceived-2",
          event: "onHeadersReceived",
          details: {
            url: getURLHttpSimpleLoad(),
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onResponseStarted",
          event: "onResponseStarted",
          details: {
            url: getURLHttpSimpleLoad(),
            statusCode: 200,
            responseHeadersExist: true,
            ip: "127.0.0.1",
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            url: getURLHttpSimpleLoad(),
            statusCode: 200,
            ip: "127.0.0.1",
            fromCache: false,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        }
      ],
      [  // event order
        ["onBeforeRequest-1", "onBeforeSendHeaders-1", "onSendHeaders-1",
         "onHeadersReceived-1", "onBeforeRedirect",
         "onBeforeRequest-2", "onBeforeSendHeaders-2", "onSendHeaders-2",
         "onHeadersReceived-2", "onResponseStarted", "onCompleted"] ],
      {urls: ["<all_urls>"]},  // filter
      ["requestHeaders", "responseHeaders"]);
    navigateAndWait(getURLHttpSimpleLoadRedirect());
  },

  // Navigates to a non-existing page.
  function nonExistingLoad() {
    expect(
      [  // events
        { label: "onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURL("does_not_exist.html"),
            frameUrl: getURL("does_not_exist.html"),
            initiator: getDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURL("does_not_exist.html"),
            fromCache: false,
            error: "net::ERR_FILE_NOT_FOUND",
            initiator: getDomain(initiators.BROWSER_INITIATED),
            // Request to chrome-extension:// url has no IP.
          }
        },
      ],
      [  // event order
        ["onBeforeRequest", "onErrorOccurred"] ]);
    navigateAndWait(getURL("does_not_exist.html"));
  },

  // Navigates to a page that has a sandboxed iframe.
  // Initiator will be opaque (serialized as "null").
  function sandboxedIframeLoad() {
    var sandboxContainer = getServerURL(
        'extensions/api_test/webrequest/simpleLoad/sandbox_container.html');
    var sandboxFrame = getServerURL(
        'extensions/api_test/webrequest/simpleLoad/sandbox_frame.html');
    var innerFrame = getURLHttpSimpleLoad();
    const kOpaqueOrigin = 'null';
    expect(
      [  // events
        { label: "onBeforeRequest-1",
          event: "onBeforeRequest",
          details: {
            url: sandboxContainer,
            frameUrl: sandboxContainer,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeSendHeaders-1",
          event: "onBeforeSendHeaders",
          details: {
            url: sandboxContainer,
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onSendHeaders-1",
          event: "onSendHeaders",
          details: {
            url: sandboxContainer,
            requestHeadersValid: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onHeadersReceived-1",
          event: "onHeadersReceived",
          details: {
            url: sandboxContainer,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onResponseStarted-1",
          event: "onResponseStarted",
          details: {
            url: sandboxContainer,
            statusCode: 200,
            responseHeadersExist: true,
            ip: "127.0.0.1",
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onCompleted-1",
          event: "onCompleted",
          details: {
            url: sandboxContainer,
            statusCode: 200,
            ip: "127.0.0.1",
            fromCache: false,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getServerDomain(initiators.BROWSER_INITIATED),
          }
        },
        { label: "onBeforeRequest-2",
          event: "onBeforeRequest",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            frameUrl: sandboxFrame,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            parentDocumentId: 1,
            parentFrameId: 0,
            url: sandboxFrame,
            type: "sub_frame",
          }
        },
        { label: "onBeforeSendHeaders-2",
          event: "onBeforeSendHeaders",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            initiator: getServerDomain(initiators.WEB_INITIATED),
            parentDocumentId: 1,
            parentFrameId: 0,
            requestHeadersValid: true,
            type: "sub_frame",
            url: sandboxFrame,
          }
        },
        { label: "onSendHeaders-2",
          event: "onSendHeaders",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            initiator: getServerDomain(initiators.WEB_INITIATED),
            parentDocumentId: 1,
            parentFrameId: 0,
            requestHeadersValid: true,
            type: "sub_frame",
            url: sandboxFrame,
          }
        },
        { label: "onHeadersReceived-2",
          event: "onHeadersReceived",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            initiator: getServerDomain(initiators.WEB_INITIATED),
            parentDocumentId: 1,
            parentFrameId: 0,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: sandboxFrame,
          }
        },
        { label: "onResponseStarted-2",
          event: "onResponseStarted",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            fromCache: false,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            ip: "127.0.0.1",
            parentDocumentId: 1,
            parentFrameId: 0,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: sandboxFrame,
          }
        },
        { label: "onCompleted-2",
          event: "onCompleted",
          details: {
            frameId: 1,
            frameType: 'sub_frame',
            fromCache: false,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            ip: "127.0.0.1",
            parentDocumentId: 1,
            parentFrameId: 0,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: sandboxFrame,
          }
        },
        { label: "onBeforeRequest-3",
          event: "onBeforeRequest",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            frameUrl: innerFrame,
            initiator: kOpaqueOrigin,
            parentDocumentId: 2,
            parentFrameId: 1,
            url: innerFrame,
            type: "sub_frame",
          }
        },
        { label: "onBeforeSendHeaders-3",
          event: "onBeforeSendHeaders",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            initiator: kOpaqueOrigin,
            parentDocumentId: 2,
            parentFrameId: 1,
            requestHeadersValid: true,
            type: "sub_frame",
            url: innerFrame,
          }
        },
        { label: "onSendHeaders-3",
          event: "onSendHeaders",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            initiator: kOpaqueOrigin,
            parentDocumentId: 2,
            parentFrameId: 1,
            requestHeadersValid: true,
            type: "sub_frame",
            url: innerFrame,
          }
        },
        { label: "onHeadersReceived-3",
          event: "onHeadersReceived",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            initiator: kOpaqueOrigin,
            parentDocumentId: 2,
            parentFrameId: 1,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: innerFrame,
          }
        },
        { label: "onResponseStarted-3",
          event: "onResponseStarted",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            fromCache: false,
            initiator: kOpaqueOrigin,
            ip: "127.0.0.1",
            parentDocumentId: 2,
            parentFrameId: 1,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: innerFrame,
          }
        },
        { label: "onCompleted-3",
          event: "onCompleted",
          details: {
            frameId: 2,
            frameType: 'sub_frame',
            fromCache: false,
            initiator: kOpaqueOrigin,
            ip: "127.0.0.1",
            parentDocumentId: 2,
            parentFrameId: 1,
            responseHeadersExist: true,
            statusLine: "HTTP/1.1 200 OK",
            statusCode: 200,
            type: "sub_frame",
            url: innerFrame,
          }
        }
      ],
      [  // event order
        ["onBeforeRequest-1", "onBeforeSendHeaders-1", "onSendHeaders-1",
         "onHeadersReceived-1", "onResponseStarted-1", "onCompleted-1",
         "onBeforeRequest-2", "onBeforeSendHeaders-2", "onSendHeaders-2",
         "onHeadersReceived-2", "onResponseStarted-2", "onCompleted-2",
         "onBeforeRequest-3", "onBeforeSendHeaders-3", "onSendHeaders-3",
         "onHeadersReceived-3", "onResponseStarted-3", "onCompleted-3"] ],
      {urls: ["<all_urls>"]},  // filter
      ["requestHeaders", "responseHeaders"]);
    navigateAndWait(sandboxContainer);
  }
])});
