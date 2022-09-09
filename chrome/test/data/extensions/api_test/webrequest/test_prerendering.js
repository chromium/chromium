// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kExtensionPath = 'extensions/api_test/webrequest/prerendering';

runTests([
  function simpleLoad() {
    const kInitiatorUrl = getServerURL(`${kExtensionPath}/initiator.html`);
    const kPrerenderingUrl =
      getServerURL(`${kExtensionPath}/prerendering.html`);
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
            documentLifecycle: 'prerender'
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
            documentLifecycle: 'prerender'
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
            documentLifecycle: 'prerender'
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
            statusLine: 'HTTP/1.1 200 OK'
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
            ip: '127.0.0.1'
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
            ip: '127.0.0.1'
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
            documentLifecycle: 'prerender'
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
            documentLifecycle: 'prerender'
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
            documentLifecycle: 'prerender'
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
            statusLine: 'HTTP/1.1 200 OK'
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
            ip: '127.0.0.1'
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
            ip: '127.0.0.1'
          }
        },
        {
          label: 'onBeforeRequest-4',
          event: 'onBeforeRequest',
          details: {
            url: kInitiatorUrl,
            frameUrl: kInitiatorUrl,
            initiator: getServerDomain(initiators.WEB_INITIATED),
            method: 'GET',
            frameId: 0,
            documentId: 1,
            parentFrameId: -1,
            frameType: 'outermost_frame',
            type: 'xmlhttprequest',
            documentLifecycle: 'active'
          }
        }
      ],
      [
        // Events
        // *-1: for navigate to the initiator page.
        // *-2: for prerendering.
        // *-3: for a script sub-resource in the prerendering page.
        // *-4: for fetch request made after the page activation.
        ['onBeforeRequest-1', 'onBeforeSendHeaders-1', 'onSendHeaders-1',
          'onHeadersReceived-1', 'onResponseStarted-1', 'onCompleted-1',
          'onBeforeRequest-2', 'onBeforeSendHeaders-2', 'onSendHeaders-2',
          'onHeadersReceived-2', 'onResponseStarted-2', 'onCompleted-2',
          'onBeforeRequest-3', 'onBeforeSendHeaders-3', 'onSendHeaders-3',
          'onHeadersReceived-3', 'onResponseStarted-3', 'onCompleted-3',
          'onBeforeRequest-4'],
      ],
      { urls: ['<all_urls>'] },  // filter
      ['requestHeaders', 'responseHeaders']);
    // Set another listener to know the pre-rendered page is ready and to
    // actrivate the page.
    const activationCallback = details => {
      chrome.test.assertEq('prerender', details.documentLifecycle);
      chrome.test.assertEq(kEmptyJsUrl, details.url);
      chrome.tabs.executeScript({
        code: `location.href = '${kPrerenderingUrl}';`,
        runAt: 'document_idle'
      });
      chrome.webRequest.onCompleted.removeListener(activationCallback);
    };
    chrome.webRequest.onCompleted.addListener(
      activationCallback, { urls: [kEmptyJsUrl] }, []);
    navigateAndWait(kInitiatorUrl);
  }
]);
