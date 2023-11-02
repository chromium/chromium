// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab(
      [
        // Tries to open a WebSocket connection, with a blocking handler that
        // cancels the request. The connection will not be established.
        function handshakeRequestCancelled() {
          var url = getWSTestURL(testWebSocketPort);
          expect(
              [
                // events
                {
                  label: 'onBeforeRequest',
                  event: 'onBeforeRequest',
                  details: {
                    url: url,
                    type: 'websocket',
                    frameUrl: 'unknown frame URL',
                    initiator: getDomain(initiators.WEB_INITIATED),
                    documentId: 1
                  },
                  retval: {cancel: true}
                },
                // Cancelling is considered an error.
                {
                  label: 'onErrorOccurred',
                  event: 'onErrorOccurred',
                  details: {
                    url: url,
                    type: 'websocket',
                    fromCache: false,
                    initiator: getDomain(initiators.WEB_INITIATED),
                    error: 'net::ERR_BLOCKED_BY_CLIENT',
                    documentId: 1
                  }
                },
              ],
              [  // event order
                ['onBeforeRequest', 'onErrorOccurred']
              ],
              {urls: ['ws://*/*']},  // filter
              ['blocking']           // extraInfoSpec
          );
          testWebSocketConnection(url, false /* expectedToConnect */);
        },
      ],
      tab);
});
