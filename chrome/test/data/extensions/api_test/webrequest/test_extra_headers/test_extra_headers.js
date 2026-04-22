// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

function getSetCookieUrl(name, value) {
  return getServerURL(
      `set-cookie?${name}` +
      `=${value}`);
}

function testModifyHeadersOnRedirect(useExtraHeaders) {
  // Use /echoheader instead of observing headers in onSendHeaders to
  // ensure we're looking at what the server receives. This avoids bugs in the
  // webRequest implementation from being masked.
  const finalURL = getServerURL('echoheader?User-Agent&Accept&X-New-Header');
  const url = getServerURL(`server-redirect?${finalURL}`);
  const listener = callbackPass(function(details) {
    const headers = details.requestHeaders;

    // Test modification.
    let acceptValue;
    for (let i = 0; i < headers.length; i++) {
      if (headers[i].name.toLowerCase() === 'user-agent') {
        headers[i].value = 'foo';
      } else if (headers[i].name.toLowerCase() === 'accept') {
        acceptValue = headers[i].value;
      }
    }

    // Test removal.
    chrome.test.assertTrue(acceptValue.indexOf('image/webp') >= 0);
    removeHeader(headers, 'accept');

    // Test addition.
    headers.push({name: 'X-New-Header', value: 'Baz'});

    return {requestHeaders: headers};
  });

  const extraInfo = ['requestHeaders', 'blocking'];
  if (useExtraHeaders) {
    extraInfo.push('extraHeaders');
  }
  chrome.webRequest.onBeforeSendHeaders.addListener(
      listener, {urls: [finalURL]}, extraInfo);

  navigateAndWait(url, function(tab) {
    chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
    chrome.tabs.executeScript(
        tab.id, {
          code: 'document.body.innerText',
        },
        callbackPass(function(results) {
          chrome.test.assertTrue(
              results[0].indexOf('foo') >= 0, 'User-Agent should be modified.');
          chrome.test.assertTrue(
              results[0].indexOf('image/webp') == -1,
              'Accept should be removed.');
          chrome.test.assertTrue(
              results[0].indexOf('Baz') >= 0, 'X-New-Header should be added.');
        }));
  });
}

const SCRIPT_URL = '_test_resources/api_test/webrequest/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  runTests([
    function testSpecialRequestHeadersVisible() {
      // Set a cookie so the cookie request header is set.
      navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
        const url = getServerURL('echo');
        const extraHeadersListener = callbackPass(function(details) {
          checkHeaders(details.requestHeaders, ['user-agent', 'cookie'], []);
        });
        chrome.webRequest.onBeforeSendHeaders.addListener(
            extraHeadersListener, {urls: [url]},
            ['requestHeaders', 'extraHeaders']);

        const standardListener = callbackPass(function(details) {
          checkHeaders(details.requestHeaders, ['user-agent'], ['cookie']);
        });
        chrome.webRequest.onBeforeSendHeaders.addListener(
            standardListener, {urls: [url]}, ['requestHeaders']);

        navigateAndWait(url, function() {
          chrome.webRequest.onBeforeSendHeaders.removeListener(
              extraHeadersListener);
          chrome.webRequest.onBeforeSendHeaders.removeListener(
              standardListener);
        });
      });
    },

    function testSpecialResponseHeadersVisible() {
      const url = getSetCookieUrl('foo', 'bar');
      let extraHeadersListenerCalledCount = 0;
      function extraHeadersListener(details) {
        extraHeadersListenerCalledCount++;
        checkHeaders(details.responseHeaders, ['set-cookie'], []);
      }
      chrome.webRequest.onHeadersReceived.addListener(
          extraHeadersListener, {urls: [url]},
          ['responseHeaders', 'extraHeaders']);
      chrome.webRequest.onResponseStarted.addListener(
          extraHeadersListener, {urls: [url]},
          ['responseHeaders', 'extraHeaders']);
      chrome.webRequest.onCompleted.addListener(
          extraHeadersListener, {urls: [url]},
          ['responseHeaders', 'extraHeaders']);

      let standardListenerCalledCount = 0;
      function standardListener(details) {
        standardListenerCalledCount++;
        checkHeaders(details.responseHeaders, [], ['set-cookie']);
      }
      chrome.webRequest.onHeadersReceived.addListener(
          standardListener, {urls: [url]}, ['responseHeaders']);
      chrome.webRequest.onResponseStarted.addListener(
          standardListener, {urls: [url]}, ['responseHeaders']);
      chrome.webRequest.onCompleted.addListener(
          standardListener, {urls: [url]}, ['responseHeaders']);

      navigateAndWait(url, function() {
        chrome.test.assertEq(3, standardListenerCalledCount);
        chrome.test.assertEq(3, extraHeadersListenerCalledCount);
        chrome.webRequest.onHeadersReceived.removeListener(
            extraHeadersListener);
        chrome.webRequest.onResponseStarted.removeListener(
            extraHeadersListener);
        chrome.webRequest.onCompleted.removeListener(extraHeadersListener);
        chrome.webRequest.onHeadersReceived.removeListener(standardListener);
        chrome.webRequest.onResponseStarted.removeListener(standardListener);
        chrome.webRequest.onCompleted.removeListener(standardListener);
      });
    },

    function testModifySpecialRequestHeaders() {
      // Set a cookie so the cookie request header is set.
      navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
        const url = getServerURL('echoheader?Cookie');
        const listener = callbackPass(function(details) {
          removeHeader(details.requestHeaders, 'cookie');
          return {requestHeaders: details.requestHeaders};
        });
        chrome.webRequest.onBeforeSendHeaders.addListener(
            listener, {urls: [url]},
            ['requestHeaders', 'blocking', 'extraHeaders']);

        navigateAndWait(url, function(tab) {
          chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
          chrome.tabs.executeScript(
              tab.id, {
                code: 'document.body.innerText',
              },
              callbackPass(function(results) {
                chrome.test.assertTrue(
                    results[0].indexOf('bar') == -1, 'Header not removed.');
              }));
        });
      });
    },

    function testModifySpecialResponseHeaders() {
      const url = getSetCookieUrl('foo', 'bar');
      const headersListener = callbackPass(function(details) {
        checkHeaders(details.responseHeaders, ['set-cookie'], []);
        details.responseHeaders.push({name: 'X-New-Header', value: 'Foo'});
        return {responseHeaders: details.responseHeaders};
      });
      chrome.webRequest.onHeadersReceived.addListener(
          headersListener, {urls: [url]},
          ['responseHeaders', 'blocking', 'extraHeaders']);

      const responseListener = callbackPass(function(details) {
        checkHeaders(
            details.responseHeaders, ['set-cookie', 'x-new-header'], []);
      });
      chrome.webRequest.onResponseStarted.addListener(
          responseListener, {urls: [url]}, ['responseHeaders', 'extraHeaders']);

      const completedListener = callbackPass(function(details) {
        checkHeaders(
            details.responseHeaders, ['set-cookie', 'x-new-header'], []);
      });
      chrome.webRequest.onCompleted.addListener(
          completedListener, {urls: [url]},
          ['responseHeaders', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onHeadersReceived.removeListener(headersListener);
        chrome.webRequest.onResponseStarted.removeListener(responseListener);
        chrome.webRequest.onCompleted.removeListener(completedListener);
      });
    },

    function testCannotModifySpecialRequestHeadersWithoutExtraHeaders() {
      // Set a cookie so the cookie request header is set.
      navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
        const url = getServerURL('echoheader?Cookie');
        const listener = callbackPass(function(details) {
          removeHeader(details.requestHeaders, 'cookie');
          return {requestHeaders: details.requestHeaders};
        });
        chrome.webRequest.onBeforeSendHeaders.addListener(
            listener, {urls: [url]}, ['requestHeaders', 'blocking']);

        // Add a no-op listener with extraHeaders to make sure it does not
        // affect the other listener.
        const noop = function() {};
        chrome.webRequest.onBeforeSendHeaders.addListener(
            noop, {urls: [url]},
            ['requestHeaders', 'blocking', 'extraHeaders']);

        navigateAndWait(url, function(tab) {
          chrome.webRequest.onBeforeSendHeaders.removeListener(noop);
          chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
          chrome.tabs.executeScript(
              tab.id, {
                code: 'document.body.innerText',
              },
              callbackPass(function(results) {
                chrome.test.assertTrue(
                    results[0].indexOf('bar') >= 0,
                    'Header should not be removed.');
              }));
        });
      });
    },

    function testModifyUserAgentWithoutExtraHeaders() {
      const url = getServerURL('echoheader?User-Agent');
      const listener = callbackPass(function(details) {
        const headers = details.requestHeaders;
        for (let i = 0; i < headers.length; i++) {
          if (headers[i].name.toLowerCase() === 'user-agent') {
            headers[i].value = 'foo';
            break;
          }
        }
        return {requestHeaders: headers};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(
          listener, {urls: [url]}, ['requestHeaders', 'blocking']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(
            tab.id, {
              code: 'document.body.innerText',
            },
            callbackPass(function(results) {
              chrome.test.assertTrue(
                  results[0].indexOf('foo') >= 0,
                  'User-Agent should be modified.');
            }));
      });
    },

    function testModifyHeadersOnRedirectWithoutExtraHeaders() {
      testModifyHeadersOnRedirect(false);
    },

    function testModifyHeadersOnRedirectWithExtraHeaders() {
      testModifyHeadersOnRedirect(true);
    },

    // Successful Set-Cookie modification is tested in test_blocking_cookie.js.
    function testCannotModifySpecialResponseHeadersWithoutExtraHeaders() {
      // Use unique name and value so other tests don't interfere.
      const url = getSetCookieUrl('theName', 'theValue');
      const listener = callbackPass(function(details) {
        removeHeader(details.responseHeaders, 'set-cookie');
        return {responseHeaders: details.responseHeaders};
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: [url]}, ['responseHeaders', 'blocking']);

      // Add a no-op listener with extraHeaders to make sure it does not affect
      // the other listener.
      const noop = function() {};
      chrome.webRequest.onHeadersReceived.addListener(
          noop, {urls: [url]}, ['responseHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onHeadersReceived.removeListener(noop);
        chrome.webRequest.onHeadersReceived.removeListener(listener);
        chrome.tabs.executeScript(
            tab.id, {
              code: 'document.cookie',
            },
            callbackPass(function(results) {
              chrome.test.assertTrue(
                  results[0].indexOf('theValue') >= 0,
                  'Header should not be removed.');
            }));
      });
    },

    function testRedirectToUrlWithExtraHeadersListener() {
      // Set a cookie so the cookie request header is set.
      navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
        const finalURL = getServerURL('echoheader?Cookie');
        const url = getServerURL(`server-redirect?${finalURL}`);
        const listener = callbackPass(function(details) {
          removeHeader(details.requestHeaders, 'cookie');
          return {requestHeaders: details.requestHeaders};
        });
        chrome.webRequest.onBeforeSendHeaders.addListener(
            listener, {urls: [finalURL]},
            ['requestHeaders', 'blocking', 'extraHeaders']);

        navigateAndWait(url, function(tab) {
          chrome.test.assertEq(finalURL, tab.url);
          chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
          chrome.tabs.executeScript(
              tab.id, {
                code: 'document.body.innerText',
              },
              callbackPass(function(results) {
                chrome.test.assertTrue(
                    results[0].indexOf('bar') == -1, 'Header not removed.');
              }));
        });
      });
    },
  ]);
});
