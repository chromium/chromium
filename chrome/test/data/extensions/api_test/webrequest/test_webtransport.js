// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab(
      [
        // Tries to open a WebTransport session and the session will be
        // established.
        async function sessionEstablished() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
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
                    // TODO(crbug.com/1243196): Return valid frame URL.
                    frameUrl: 'unknown frame URL',
                    initiator: getDomain(initiators.WEB_INITIATED)
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    initiator: getDomain(initiators.WEB_INITIATED),
                  },
                },
                {
                  label: 'onSendHeaders',
                  event: 'onSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    initiator: getDomain(initiators.WEB_INITIATED),
                  },
                },
              ],
              [  // event order
                [
                  'onBeforeRequest',
                  'onBeforeSendHeaders',
                  'onSendHeaders',
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
                    frameUrl: 'unknown frame URL',
                    initiator: getDomain(initiators.WEB_INITIATED)
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
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT'
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
                    frameUrl: 'unknown frame URL',
                    initiator: getDomain(initiators.WEB_INITIATED)
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    initiator: getDomain(initiators.WEB_INITIATED)
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
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT'
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
          const onBeforeSendHeaders = callbackPass((details) => {
            const headers = details.requestHeaders;
            chrome.test.assertEq([], headers);
            headers['foo'] = 'bar';

            chrome.webRequest.onBeforeSendHeaders.removeListener(
                onBeforeSendHeaders);
          });
          chrome.webRequest.onBeforeSendHeaders.addListener(
              onBeforeSendHeaders, {urls: [url]}, ['requestHeaders']);

          const onSendHeaders = callbackPass((details) => {
            const headers = details.requestHeaders;
            // Header mutation in onBeforeSendHeaders is ignored.
            chrome.test.assertEq([], headers);

            chrome.webRequest.onSendHeaders.removeListener(onSendHeaders);
          });
          chrome.webRequest.onSendHeaders.addListener(
              onSendHeaders, {urls: [url]}, ['requestHeaders']);

          const done = chrome.test.callbackAdded();
          await expectSessionEstablished(url);
          done();
        },

        // Tries to open a WebTransport session with invalid request.
        // The connection will not be established.
        async function serverRejected() {
          const url = `https://localhost:${testWebTransportPort}/invalid`;

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
                    frameUrl: 'unknown frame URL',
                    initiator: getDomain(initiators.WEB_INITIATED)
                  },
                },
                {
                  label: 'onBeforeSendHeaders',
                  event: 'onBeforeSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    initiator: getDomain(initiators.WEB_INITIATED)
                  },
                },
                {
                  label: 'onSendHeaders',
                  event: 'onSendHeaders',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    initiator: getDomain(initiators.WEB_INITIATED)
                  },
                },
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    method: 'CONNECT',
                    url: url,
                    type: 'webtransport',
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_ABORTED'
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

      ],
      tab);
});
