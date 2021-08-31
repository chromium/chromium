// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab(
      [
        // Tries to open a WebTransport session and the connection will be
        // established.
        async function connectionEstablished() {
          const url = `https://localhost:${testWebTransportPort}/echo`;
          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    method: 'GET',
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
          const transport = new WebTransport(url);
          const done = chrome.test.callbackAdded();
          await transport.ready.catch((e) => {
            chrome.test.fail('Ready rejected: ${e}');
          });
          done();
        },

        // Tries to open a WebTransport session, with an onBeforeRequest
        // blocking handler that cancels the request. The connection will not be
        // established.
        async function handshakeRequestCancelled() {
          const url = `https://localhost:${testWebTransportPort}/echo`;

          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
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
          const transport = new WebTransport(url);
          const done = chrome.test.callbackAdded();
          try {
            await transport.ready;
            chrome.test.fail('Ready should be rejected.');
          } catch (e) {
            // TODO(crbug.com/1240935): Consider showing error.
            // This is filtered by InterceptingHandshakeClient.
            chrome.test.assertEq({}, e);
            done();
          }
        },

      ],
      tab);
});
