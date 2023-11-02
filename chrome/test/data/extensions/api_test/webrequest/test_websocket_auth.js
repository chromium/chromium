// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.tabs.getCurrent(function(tab) {
    runTestsForTab(
        [
          // onAuthRequired is not a blocking function.
          function webSocketAuthRequiredNonBlocking() {
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
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onBeforeSendHeaders',
                    event: 'onBeforeSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onSendHeaders',
                    event: 'onSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onHeadersReceived',
                    event: 'onHeadersReceived',
                    details: {
                      url: url,
                      type: 'websocket',
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      responseHeadersExist: true,
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onAuthRequired',
                    event: 'onAuthRequired',
                    details: {
                      url: url,
                      type: 'websocket',
                      isProxy: false,
                      scheme: 'basic',
                      realm: 'Pywebsocket',
                      challenger: {host: 'localhost', port: testWebSocketPort},
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      responseHeadersExist: true,
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onErrorOccurred',
                    event: 'onErrorOccurred',
                    details: {
                      url: url,
                      type: 'websocket',
                      ip: '127.0.0.1',
                      fromCache: false,
                      error: 'net::ERR_FAILED',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                ],
                [  // event order
                  [
                    'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                    'onHeadersReceived', 'onAuthRequired', 'onErrorOccurred'
                  ]
                ],
                {urls: ['<all_urls>']},  // filter
                ['responseHeaders']      // extraInfoSpec
            );
            testWebSocketConnection(url, false /* expectedToConnect*/);
          },

          // onAuthRequired is a blocking function but takes no action.
          function webSocketAuthRequiredSyncNoAction() {
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
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onBeforeSendHeaders',
                    event: 'onBeforeSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onSendHeaders',
                    event: 'onSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onHeadersReceived',
                    event: 'onHeadersReceived',
                    details: {
                      url: url,
                      type: 'websocket',
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onAuthRequired',
                    event: 'onAuthRequired',
                    details: {
                      url: url,
                      type: 'websocket',
                      isProxy: false,
                      scheme: 'basic',
                      realm: 'Pywebsocket',
                      challenger: {host: 'localhost', port: testWebSocketPort},
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onErrorOccurred',
                    event: 'onErrorOccurred',
                    details: {
                      url: url,
                      type: 'websocket',
                      ip: '127.0.0.1',
                      fromCache: false,
                      error: 'net::ERR_FAILED',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                ],
                [  // event order
                  [
                    'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                    'onHeadersReceived', 'onAuthRequired', 'onErrorOccurred'
                  ]
                ],
                {urls: ['<all_urls>']}, ['blocking']);
            testWebSocketConnection(url, false /* expectedToConnect*/);
          },

          // onAuthRequired is a blocking function that cancels the auth
          // attempt.
          function webSocketAuthRequiredSyncCancelAuth() {
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
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onBeforeSendHeaders',
                    event: 'onBeforeSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onSendHeaders',
                    event: 'onSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onHeadersReceived',
                    event: 'onHeadersReceived',
                    details: {
                      url: url,
                      type: 'websocket',
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onAuthRequired',
                    event: 'onAuthRequired',
                    details: {
                      url: url,
                      type: 'websocket',
                      isProxy: false,
                      scheme: 'basic',
                      realm: 'Pywebsocket',
                      challenger: {host: 'localhost', port: testWebSocketPort},
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                    retval: {cancel: true}
                  },
                  {
                    label: 'onErrorOccurred',
                    event: 'onErrorOccurred',
                    details: {
                      url: url,
                      type: 'websocket',
                      ip: '127.0.0.1',
                      fromCache: false,
                      error: 'net::ERR_FAILED',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                ],
                [  // event order
                  [
                    'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                    'onHeadersReceived', 'onAuthRequired', 'onErrorOccurred'
                  ]
                ],
                {urls: ['<all_urls>']}, ['blocking']);
            testWebSocketConnection(url, false /* expectedToConnect*/);
          },

          // onAuthRequired is a blocking function setting authentication
          // credentials.
          function webSocketAuthRequiredSyncSetAuth() {
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
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onBeforeSendHeaders',
                    event: 'onBeforeSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onSendHeaders',
                    event: 'onSendHeaders',
                    details: {
                      url: url,
                      type: 'websocket',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onHeadersReceived',
                    event: 'onHeadersReceived',
                    details: {
                      url: url,
                      type: 'websocket',
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                  {
                    label: 'onAuthRequired',
                    event: 'onAuthRequired',
                    details: {
                      url: url,
                      type: 'websocket',
                      isProxy: false,
                      scheme: 'basic',
                      realm: 'Pywebsocket',
                      challenger: {host: 'localhost', port: testWebSocketPort},
                      statusCode: 401,
                      statusLine: 'HTTP/1.0 401 Unauthorized',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                    // Note: The test WebSocket server accepts only these
                    // credentials.
                    retval:
                        {authCredentials: {username: 'test', password: 'test'}}
                  },
                  {
                    label: 'onResponseStarted',
                    event: 'onResponseStarted',
                    details: {
                      url: url,
                      type: 'websocket',
                      ip: '127.0.0.1',
                      fromCache: false,
                      statusCode: 101,
                      statusLine: 'HTTP/1.1 101 Switching Protocols',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    },
                  },
                  {
                    label: 'onCompleted',
                    event: 'onCompleted',
                    details: {
                      url: url,
                      type: 'websocket',
                      ip: '127.0.0.1',
                      fromCache: false,
                      statusCode: 101,
                      statusLine: 'HTTP/1.1 101 Switching Protocols',
                      initiator: getDomain(initiators.WEB_INITIATED)
                    }
                  },
                ],
                [  // event order
                  [
                    'onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
                    'onHeadersReceived', 'onAuthRequired', 'onResponseStarted',
                    'onCompleted'
                  ]
                ],
                {urls: ['<all_urls>']}, ['blocking']);
            testWebSocketConnection(url, true /* expectedToConnect*/);
          },
        ],
        tab);
  });
});
