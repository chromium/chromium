// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

// Constants as functions, not to be called until after runTests.
function getURLHttpWithScript() {
  return getServerURL('extensions/api_test/webrequest/script/index.html');
}

function getURLScript() {
  return getServerURL('extensions/api_test/webrequest/script/test.js');
}

runTests([
  // Navigates to a page with subresources, with a blocking handler that
  // cancels one.
  function simpleLoadCancelledOnReceiveHeaders() {
    expect(
        [
          // events
          {
            label: 'onBeforeRequest',
            event: 'onBeforeRequest',
            details: {
              method: 'GET',
              type: 'main_frame',
              url: getURLHttpWithScript(),
              frameUrl: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED)
            },
          },
          {
            label: 'onBeforeSendHeaders',
            event: 'onBeforeSendHeaders',
            details: {
              url: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED)
              // Note: no requestHeaders because we don't ask for them.
            },
          },
          {
            label: 'onSendHeaders',
            event: 'onSendHeaders',
            details: {
              url: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED)
            }
          },
          {
            label: 'onHeadersReceived',
            event: 'onHeadersReceived',
            details: {
              url: getURLHttpWithScript(),
              statusLine: 'HTTP/1.1 200 OK',
              statusCode: 200,
              initiator: getServerDomain(initiators.BROWSER_INITIATED)
            },
          },
          {
            label: 'onResponseStarted',
            event: 'onResponseStarted',
            details: {
              url: getURLHttpWithScript(),
              statusLine: 'HTTP/1.1 200 OK',
              statusCode: 200,
              ip: '127.0.0.1',
              fromCache: false
            },
          },
          {
            label: 'onCompleted',
            event: 'onCompleted',
            details: {
              url: getURLHttpWithScript(),
              statusLine: 'HTTP/1.1 200 OK',
              statusCode: 200,
              ip: '127.0.0.1',
              fromCache: false
            },
          },
          {
            label: 'onBeforeRequest-script',
            event: 'onBeforeRequest',
            details: {
              method: 'GET',
              type: 'script',
              url: getURLScript(),
              frameUrl: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
            retval: {cancel: true}
          },
          {
            label: 'onErrorOccurred-script',
            event: 'onErrorOccurred',
            details: {
              url: getURLScript(),
              initiator: getServerDomain(initiators.WEB_INITIATED),
              error: 'net::ERR_BLOCKED_BY_CLIENT',
              fromCache: false,
              type: 'script',
              documentId: 1
            },
          },
        ],
        [  // event order
          [
            'onBeforeRequest',
            'onBeforeSendHeaders',
            'onSendHeaders',
            'onHeadersReceived',
            'onResponseStarted',
            'onCompleted',
            'onBeforeRequest-script',
            'onErrorOccurred-script',
          ]
        ],
        {urls: ['<all_urls>']},  // filter
        ['blocking']);
    navigateAndWait(getURLHttpWithScript());
  },
]);
