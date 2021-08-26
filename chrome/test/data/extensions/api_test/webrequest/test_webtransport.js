// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab(
      [
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
      ],
      tab);
});
