// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
              ],
              [  // event order
                [
                  'onBeforeRequest',
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
                ['onBeforeRequest', 'onErrorOccurred']
              ],
              {urls: ['https://*/*']},  // filter
              ['blocking']              // extraInfoSpec
          );

          const transport = new WebTransport(url);
          const done = chrome.test.callbackAdded();
          try {
            await transport.ready;
            chrome.test.fail('Ready should be rejected.');
          } catch (e) {
            chrome.test.assertEq({}, e);
            done();
          }
        },

      ],
      tab);
});
