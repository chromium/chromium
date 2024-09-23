// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants as functions, not to be called until after runTests.
function getURLSetCookie() {
  return getServerURL('set-cookie?Foo=Bar');
}

function getURLNonUTF8SetCookie() {
  return getServerURL('set-header?Set-Cookie%3A%20Foo%3D%FE%D1');
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Loads a testserver page that sets a cookie "Foo=Bar" but removes
  // the cookies from the response headers so that they are not set.
  function modifyResponseCookieHeader() {
    expect(
      [  // events
        { label: 'onBeforeRequest',
          event: 'onBeforeRequest',
          details: {
            method: 'GET',
            type: 'main_frame',
            url: getURLSetCookie(),
            frameUrl: getURLSetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onBeforeSendHeaders',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLSetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
            // Note: no requestHeaders because we don't ask for them.
          },
        },
        { label: 'onSendHeaders',
          event: 'onSendHeaders',
          details: {
            url: getURLSetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onHeadersReceived',
          event: 'onHeadersReceived',
          details: {
            url: getURLSetCookie(),
            statusLine: 'HTTP/1.1 200 OK',
            statusCode: 200,
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
          retval_function: function(name, details) {
            responseHeaders = details.responseHeaders;
            var found = false;
            for (var i = 0; i < responseHeaders.length; ++i) {
              if (responseHeaders[i].name === 'Set-Cookie' &&
                  responseHeaders[i].value.indexOf('Foo') != -1) {
                found = true;
                responseHeaders.splice(i);
                break;
              }
            }
            chrome.test.assertTrue(found);
            return {responseHeaders: responseHeaders};
          }
        },
        { label: 'onResponseStarted',
          event: 'onResponseStarted',
          details: {
            url: getURLSetCookie(),
            fromCache: false,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onCompleted',
          event: 'onCompleted',
          details: {
            url: getURLSetCookie(),
            fromCache: false,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [  // event order
        ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
         'onHeadersReceived', 'onResponseStarted', 'onCompleted']
      ],
      {urls: ['<all_urls>']}, ['blocking', 'responseHeaders', 'extraHeaders']);
    // Check that the cookie was really removed.
    navigateAndWait(getURLSetCookie(), function() {
      chrome.test.listenOnce(chrome.runtime.onMessage, function(request) {
        chrome.test.assertTrue(request.pass, 'Cookie was not removed.');
      });
      chrome.tabs.executeScript(tabId,
      { code: 'chrome.runtime.sendMessage(' +
            '{pass: document.cookie.indexOf("Foo") == -1});'
        });
    });
  },

  // Loads a testserver page that sets a cookie 'Foo=U+FDD1' which is not a
  // valid UTF-8 code point. Therefore, it cannot be passed to JavaScript
  // as a normal string.
  function handleNonUTF8InModifyResponseCookieHeader() {
    expect(
      [  // events
        { label: 'onBeforeRequest',
          event: 'onBeforeRequest',
          details: {
            method: 'GET',
            type: 'main_frame',
            url: getURLNonUTF8SetCookie(),
            frameUrl: getURLNonUTF8SetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onBeforeSendHeaders',
          event: 'onBeforeSendHeaders',
          details: {
            url: getURLNonUTF8SetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
            // Note: no requestHeaders because we don't ask for them.
          },
        },
        { label: 'onSendHeaders',
          event: 'onSendHeaders',
          details: {
            url: getURLNonUTF8SetCookie(),
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onHeadersReceived',
          event: 'onHeadersReceived',
          details: {
            url: getURLNonUTF8SetCookie(),
            statusLine: 'HTTP/1.1 200 OK',
            statusCode: 200,
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          },
          retval_function: function(name, details) {
            responseHeaders = details.responseHeaders;
            var found = false;
            var expectedValue = [
              'F'.charCodeAt(0),
              'o'.charCodeAt(0),
              'o'.charCodeAt(0),
              0x3D, 0xFE, 0xD1
              ];

            for (var i = 0; i < responseHeaders.length; ++i) {
              if (responseHeaders[i].name === 'Set-Cookie' &&
                  deepEq(responseHeaders[i].binaryValue, expectedValue)) {
                found = true;
                responseHeaders.splice(i);
                break;
              }
            }
            chrome.test.assertTrue(found);
            return {responseHeaders: responseHeaders};
          }
        },
        { label: 'onResponseStarted',
          event: 'onResponseStarted',
          details: {
            url: getURLNonUTF8SetCookie(),
            fromCache: false,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: 'onCompleted',
          event: 'onCompleted',
          details: {
            url: getURLNonUTF8SetCookie(),
            fromCache: false,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            ip: '127.0.0.1',
            responseHeadersExist: true,
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
          }
        },
      ],
      [  // event order
        ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
         'onHeadersReceived', 'onResponseStarted', 'onCompleted']
      ],
      {urls: ['<all_urls>']}, ['blocking', 'responseHeaders', 'extraHeaders']);
    // Check that the cookie was really removed.
    navigateAndWait(getURLNonUTF8SetCookie(), function() {
      chrome.test.listenOnce(chrome.runtime.onMessage, function(request) {
        chrome.test.assertTrue(request.pass, 'Cookie was not removed.');
      });
      chrome.tabs.executeScript(tabId,
      { code: 'chrome.runtime.sendMessage(' +
            '{pass: document.cookie.indexOf("Foo") == -1});'
        });
    });
  },
  ])
});
