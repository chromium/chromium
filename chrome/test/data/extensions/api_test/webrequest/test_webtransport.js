// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;
const hasFrame =
    !('isSharedWorkerTest' in self) && !('isServiceWorkerTest' in self);

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab(
      [
        // Tries to open a WebTransport session and the session will be
        // established.
        async function sessionEstablished() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
          const frameId = hasFrame ? 0 : -1;
          const tabId = hasFrame ? 0 : -1;
          const documentId = hasFrame ? 1 : undefined;
          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameUrl: url,
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onSendHeaders',
                  event: 'onSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onHeadersReceived',
                  event: 'onHeadersReceived',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    statusCode: 200,
                    statusLine: 'HTTP/1.1 200',
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onResponseStarted',
                  event: 'onResponseStarted',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    statusCode: 200,
                    statusLine: 'HTTP/1.1 200',
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onCompleted',
                  event: 'onCompleted',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    statusCode: 200,
                    statusLine: 'HTTP/1.1 200',
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
              ],
              [  // event order
                [
                  'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                  'onHeadersReceived', 'onResponseStarted', 'onCompleted'
                ]
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );
          const done = chrome.test.callbackAdded();
          await expectSessionEstablished(url);
          done();
        },

        // Tries to open a WebTransport session, with an onBeforeRequest
        // blocking handler that cancels the request. The connection will not be
        // established.
        async function blockedByOnBeforeRequest() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
          const frameId = hasFrame ? 0 : -1;
          const tabId = hasFrame ? 0 : -1;
          const documentId = hasFrame ? 1 : undefined;

          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    frameUrl: url,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                  retval: {cancel: true}
                },
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT',
                    documentId: documentId
                  }
                },
              ],
              [  // event order
                ['onBeforeRequest', 'onErrorOccurred']
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );
          const done = chrome.test.callbackAdded();
          await expectSessionFailed(url);
          done();
        },

        // The handshake is cancelled in onBeforeSendHeaders.
        async function blockedByOnBeforeSendHeaders() {
          const url = `https://localhost:${testWebTransportPort}/invalid`;
          const frameId = hasFrame ? 0 : -1;
          const tabId = hasFrame ? 0 : -1;
          const documentId = hasFrame ? 1 : undefined;
          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    frameUrl: url,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                  retval: {cancel: true}
                },
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT',
                    documentId: documentId
                  }
                },
              ],
              [  // event order
                ['onBeforeRequest', 'onBeforeSendHeaders', 'onErrorOccurred']
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );

          const done = chrome.test.callbackAdded();
          await expectSessionFailed(url);
          done();
        },

        // Checks headers passed to onBeforeSendHeaders and onSendHeaders.
        async function headersInOnBeforeSendHeaders() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
          let isOnBeforeSendHeadersCalled = false;
          const onBeforeSendHeaders = callbackPass((details) => {
            const headers = details.requestHeaders;
            chrome.test.assertEq(
                [{name: 'sec-webtransport-http3-draft02', value: '1'}],
                headers);
            headers.push({name: 'foo', value: 'bar'});
            isOnBeforeSendHeadersCalled = true;

            chrome.webRequest.onBeforeSendHeaders.removeListener(
                onBeforeSendHeaders);
          });
          chrome.webRequest.onBeforeSendHeaders.addListener(
              onBeforeSendHeaders, {urls: [url]}, ['requestHeaders']);

          let isOnSendHeadersCalled = false;
          const onSendHeaders = callbackPass((details) => {
            const headers = details.requestHeaders;
            // Header mutation in onBeforeSendHeaders is ignored.
            chrome.test.assertEq(
                [{name: 'sec-webtransport-http3-draft02', value: '1'}],
                headers);
            isOnSendHeadersCalled = true;

            chrome.webRequest.onSendHeaders.removeListener(onSendHeaders);
          });
          chrome.webRequest.onSendHeaders.addListener(
              onSendHeaders, {urls: [url]}, ['requestHeaders']);

          const completed = new Promise((resolve) => {
            const onCompleted = callbackPass((details) => {
              chrome.webRequest.onCompleted.removeListener(onCompleted);
              resolve();
            });
            chrome.webRequest.onCompleted.addListener(
                onCompleted,
                {urls: [url]},
            );
          });

          const done = chrome.test.callbackAdded();
          await expectSessionEstablished(url);
          await completed;
          chrome.test.assertTrue(isOnBeforeSendHeadersCalled);
          chrome.test.assertTrue(isOnSendHeadersCalled);
          done();
        },

        // Tries to open a WebTransport session with invalid request.
        // The connection will not be established.
        async function serverRejected() {
          const url = `https://localhost:${testWebTransportPort}/invalid`;
          const frameId = hasFrame ? 0 : -1;
          const tabId = hasFrame ? 0 : -1;
          const documentId = hasFrame ? 1 : undefined;

          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    frameUrl: url,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onSendHeaders',
                  event: 'onSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_METHOD_NOT_SUPPORTED',
                    documentId: documentId
                  }
                },
              ],
              [  // event order
                [
                  'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                  'onErrorOccurred'
                ]
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );

          const done = chrome.test.callbackAdded();
          expectSessionFailed(url);
          done();
        },

        // Tries to open a WebTransport session, with an onHeadersReceived
        // blocking handler that cancels the request. The connection will not be
        // established.
        async function blockedByOnHeadersReceived() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
          const frameId = hasFrame ? 0 : -1;
          const tabId = hasFrame ? 0 : -1;
          const documentId = hasFrame ? 1 : undefined;

          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    frameUrl: url,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onSendHeaders',
                  event: 'onSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                },
                {
                  label: 'onHeadersReceived',
                  event: 'onHeadersReceived',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    statusCode: 200,
                    statusLine: 'HTTP/1.1 200',
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: documentId
                  },
                  retval: {cancel: true}
                },
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    frameId: frameId,
                    tabId: tabId,
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT',
                    documentId: documentId
                  }
                },
              ],
              [  // event order
                [
                  'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                  'onHeadersReceived', 'onErrorOccurred'
                ]
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );
          const done = chrome.test.callbackAdded();
          await expectSessionFailed(url);
          done();
        },

        // Checks headers passed to onHeadersReceived and onResponseStarted.
        async function headersInOnHeadersReceived() {
          const url = `https://localhost:${testWebTransportPort}/echo?` +
              'set-header=foo:baz';
          let isOnHeadersReceivedCalled = false;
          const onHeadersReceived = callbackPass((details) => {
            const headers = details.responseHeaders;
            chrome.test.assertEq([{name: 'foo', value: 'baz'}], headers);
            headers[0].value = 'bar';
            isOnHeadersReceivedCalled = true;

            return {responseHeaders: headers};
          });
          chrome.webRequest.onHeadersReceived.addListener(
              onHeadersReceived, {urls: [url]},
              ['blocking', 'responseHeaders']);

          let isOnResponseStartedCalled = false;
          const onResponseStarted = callbackPass((details) => {
            const headers = details.responseHeaders;
            chrome.test.assertEq([{name: 'foo', value: 'bar'}], headers);
            isOnResponseStartedCalled = true;

            chrome.webRequest.onHeadersReceived.removeListener(
                onHeadersReceived);
            chrome.webRequest.onResponseStarted.removeListener(
                onResponseStarted);
          });
          chrome.webRequest.onResponseStarted.addListener(
              onResponseStarted, {urls: [url]}, ['responseHeaders']);

          const completed = new Promise((resolve) => {
            const onCompleted = callbackPass((details) => {
              chrome.webRequest.onCompleted.removeListener(onCompleted);
              resolve();
            });
            chrome.webRequest.onCompleted.addListener(
                onCompleted,
                {urls: [url]},
            );
          });

          const done = chrome.test.callbackAdded();
          await expectSessionEstablished(url);
          await completed;
          chrome.test.assertTrue(isOnHeadersReceivedCalled);
          chrome.test.assertTrue(isOnResponseStartedCalled);
          done();
        },
      ],
      tab);
});
