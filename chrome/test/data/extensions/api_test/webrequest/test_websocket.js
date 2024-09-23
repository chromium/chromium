// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

chrome.tabs.getCurrent(function(tab) {
  runTestsForTab([
    // Opens a WebSocket connection, writes a message to it, and closes the
    // connection. WebRequest API should observe the entire handshake.
    function handshakeSucceeds() {
      var url = getWSTestURL(testWebSocketPort);
      expect(
        [  //events
          { label: 'onBeforeRequest',
            event: 'onBeforeRequest',
            details: {
              url: url,
              type: 'websocket',
              // TODO(pkalinnikov): Figure out why the frame URL is unknown.
              frameUrl: 'unknown frame URL',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onBeforeSendHeaders',
            event: 'onBeforeSendHeaders',
            details: {
              url: url,
              type: 'websocket',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onSendHeaders',
            event: 'onSendHeaders',
            details: {
              url: url,
              type: 'websocket',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onHeadersReceived',
            event: 'onHeadersReceived',
            details: {
              url: url,
              type: 'websocket',
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onResponseStarted',
            event: 'onResponseStarted',
            details: {
              url: url,
              type: 'websocket',
              ip: '127.0.0.1',
              fromCache: false,
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onCompleted',
            event: 'onCompleted',
            details: {
              url: url,
              type: 'websocket',
              ip: '127.0.0.1',
              fromCache: false,
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            }
          },
        ],
        [  // event order
          ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
          'onHeadersReceived', 'onResponseStarted', 'onCompleted']
        ],
        {urls: ['ws://*/*']},  // filter
        ['blocking']  // extraInfoSpec
      );
      testWebSocketConnection(url, true /* expectedToConnect */);
    },

    // Tries to open a WebSocket connection, with a blocking handler that
    // cancels the request. The connection will not be established.
    function handshakeRequestCancelled() {
      var url = getWSTestURL(testWebSocketPort);
      expect(
        [  // events
          { label: 'onBeforeRequest',
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
          { label: 'onErrorOccurred',
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
        ['blocking']  // extraInfoSpec
      );
      testWebSocketConnection(url, false /* expectedToConnect */);
    },

    // Opens a WebSocket connection, with a blocking handler that tries to
    // redirect the request. The redirect will be ignored.
    function redirectIsIgnoredAndHandshakeSucceeds() {
      var url = getWSTestURL(testWebSocketPort);
      var redirectedUrl1 = getWSTestURL(testWebSocketPort) + '?redirected1';
      var redirectedUrl2 = getWSTestURL(testWebSocketPort) + '?redirected2';
      expect(
        [  // events
          { label: 'onBeforeRequest',
            event: 'onBeforeRequest',
            details: {
              url: url,
              type: 'websocket',
              frameUrl: 'unknown frame URL',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
            retval: {redirectUrl: redirectedUrl1}
          },
          { label: 'onBeforeSendHeaders',
            event: 'onBeforeSendHeaders',
            details: {
              url: url,
              type: 'websocket',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onSendHeaders',
            event: 'onSendHeaders',
            details: {
              url: url,
              type: 'websocket',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
          },
          { label: 'onHeadersReceived',
            event: 'onHeadersReceived',
            details: {
              url: url,
              type: 'websocket',
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              initiator: getDomain(initiators.WEB_INITIATED),
              documentId: 1
            },
            retval: {redirectUrl: redirectedUrl2}
          },
          { label: 'onResponseStarted',
            event: 'onResponseStarted',
            details: {
              url: url,
              type: 'websocket',
              ip: '127.0.0.1',
              fromCache: false,
              initiator: getDomain(initiators.WEB_INITIATED),
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              documentId: 1
            },
          },
          { label: 'onCompleted',
            event: 'onCompleted',
            details: {
              url: url,
              type: 'websocket',
              ip: '127.0.0.1',
              fromCache: false,
              initiator: getDomain(initiators.WEB_INITIATED),
              statusCode: 101,
              statusLine: 'HTTP/1.1 101 Switching Protocols',
              documentId: 1
            }
          },
        ],
        [  // event order
          ['onBeforeRequest', 'onBeforeSendHeaders', 'onHeadersReceived',
          'onResponseStarted', 'onCompleted']
        ],
        {urls: ['ws://*/*']},  // filter
        ['blocking']  // extraInfoSpec
      );
      testWebSocketConnection(url, true /* expectedToConnect */);
    },

    // Tries to open a WebSocket connection, with a blocking handler that
    // cancels the request. The connection will not be established.
    function handshakeRequestCancelledWithExtraHeaders() {
      var url = getWSTestURL(testWebSocketPort);
      expect(
        [  // events
          { label: 'onBeforeRequest',
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
          { label: 'onErrorOccurred',
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
        ['blocking', 'extraHeaders']  // extraInfoSpec
      );
      testWebSocketConnection(url, false /* expectedToConnect */);
    },

    // Tests that all the requests headers that are added by net/ are visible
    // if extraHeaders is specified.
    function testExtraRequestHeadersVisible() {
      var url = getWSTestURL(testWebSocketPort);

      var extraHeadersListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders,
                     ['user-agent', 'accept-language'], []);
        chrome.webRequest.onBeforeSendHeaders.removeListener(
            extraHeadersListener);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(extraHeadersListener,
          {urls: [url]}, ['requestHeaders', 'extraHeaders']);

      var standardListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders,
                     ['user-agent'], ['accept-language']);
        chrome.webRequest.onBeforeSendHeaders.removeListener(standardListener);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(standardListener,
          {urls: [url]}, ['requestHeaders']);

      testWebSocketConnection(url, true /* expectedToConnect */);
    },

    // Ensure that request headers which are added by net/ could be modified if
    // the listener uses extraHeaders.
    function testModifyRequestHeaders() {
      var url = getWSTestURL(testWebSocketPort);

      var beforeSendHeadersListener = callbackPass(function(details) {
        // Test removal.
        removeHeader(details.requestHeaders, 'accept-language');

        // Test modification.
        for (var i = 0; i < details.requestHeaders.length; i++) {
          if (details.requestHeaders[i].name == 'User-Agent') {
            details.requestHeaders[i].value = 'Foo';
          }
        }

        // Test addition.
        details.requestHeaders.push({name: 'X-New-Header',
                                     value: 'Bar'});

        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(
          beforeSendHeadersListener,
          {urls: [url]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      var sendHeadersListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders, ['x-new-header'],
                     ['accept-language']);

        var seen = false;
        for (var i = 0; i < details.requestHeaders.length; i++) {
          if (details.requestHeaders[i].name == 'User-Agent') {
            chrome.test.assertEq(details.requestHeaders[i].value, 'Foo');
            seen = true;
          }
        }
        chrome.test.assertTrue(seen);

        chrome.webRequest.onBeforeSendHeaders.removeListener(
            beforeSendHeadersListener);
        chrome.webRequest.onSendHeaders.removeListener(sendHeadersListener);
      });
      chrome.webRequest.onSendHeaders.addListener(sendHeadersListener,
          {urls: [url]}, ['requestHeaders', 'extraHeaders']);

      testWebSocketConnection(url, true /* expectedToConnect */);
    },

    // Ensure that response headers can be modified when extraHeaders is used.
    function testModifyResponseHeaders() {
      var url = getWSTestURL(testWebSocketPort);

      var onHeadersReceivedHeadersListener = callbackPass(function(details) {
        // Test addition.
        details.responseHeaders.push({name: 'X-New-Header',
                                      value: 'Bar'});

        return {responseHeaders: details.responseHeaders};
      });
      chrome.webRequest.onHeadersReceived.addListener(
          onHeadersReceivedHeadersListener,
          {urls: [url]}, ['responseHeaders', 'blocking', 'extraHeaders']);

      var onResponseStartedListener = callbackPass(function(details) {
        checkHeaders(details.responseHeaders, ['x-new-header'], []);

        chrome.webRequest.onHeadersReceived.removeListener(
            onHeadersReceivedHeadersListener);
        chrome.webRequest.onResponseStarted.removeListener(
            onResponseStartedListener);
      });
      chrome.webRequest.onResponseStarted.addListener(onResponseStartedListener,
          {urls: [url]}, ['responseHeaders', 'extraHeaders']);

      testWebSocketConnection(url, true /* expectedToConnect */);
    },
  ], tab);
});
