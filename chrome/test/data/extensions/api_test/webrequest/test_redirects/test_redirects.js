// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const dataURL = 'data:text/plain,redirected1';
const aboutURL = 'about:blank';

function getURLNonWebAccessible() {
  return getURL('manifest.json');
}

function getURLWebAccessible() {
  return getURL('simpleLoad/a.html');
}

function assertRedirectSucceeds(url, redirectURL, callback) {
  navigateAndWait(url, function(tab) {
    if (callback) {
      callback();
    }
    chrome.test.assertEq(redirectURL, tab.url);
  });
}

function assertRedirectFails(url) {
  navigateAndWait(url, function(tab) {
    // Tab URL should still be set to the original URL.
    chrome.test.assertEq(url, tab.url);
  });
}

const SCRIPT_URL = '_test_resources/api_test/webrequest/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.getConfig(function(config) {
    const onHeadersReceivedExtraInfoSpec = ['blocking'];
    if (config.customArg) {
      const args = JSON.parse(config.customArg);
      if (args.useExtraHeaders) {
        onHeadersReceivedExtraInfoSpec.push('extraHeaders');
      }
    }

    runTests([
      function redirectToDataUrlOnHeadersReceived() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: dataURL};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        assertRedirectSucceeds(url, dataURL, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectToAboutUrlOnHeadersReceived() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: aboutURL};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        assertRedirectSucceeds(url, aboutURL, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectToNonWebAccessibleUrlOnHeadersReceived() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: getURLNonWebAccessible()};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectToServerRedirectOnHeadersReceived() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        assertRedirectSucceeds(url, getURLWebAccessible(), function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function serverRedirectThenExtensionRedirectOnHeadersReceived() {
        const url1 = getServerURL('echo');
        const url2 = getURLWebAccessible();
        const serverRedirect = getServerURL(`server-redirect?${url1}`);
        const listener = function(details) {
          return {redirectUrl: url2};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener,
            {urls: [url1]},
            ['blocking'],
        );

        assertRedirectSucceeds(serverRedirect, url2, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectToUnallowedServerRedirectOnHeadersReceived() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLNonWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        // The page should be redirected to the non web accessible URL, but this
        // URL will not load.
        assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectToDataUrlOnBeforeRequest() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: dataURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        assertRedirectSucceeds(url, dataURL, function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function redirectToAboutUrlOnBeforeRequest() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: aboutURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        assertRedirectSucceeds(url, aboutURL, function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function redirectToNonWebAccessibleUrlOnBeforeRequest() {
        const url = getServerURL('echo');
        const listener = function(details) {
          return {redirectUrl: getURLNonWebAccessible()};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function redirectToServerRedirectOnBeforeRequest() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        assertRedirectSucceeds(url, getURLWebAccessible(), function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      // A server redirect immediately followed by an extension redirect.
      // Regression test for:
      // - https://crbug.com/41412957
      // - https://crbug.com/41411836
      function serverRedirectThenExtensionRedirectOnBeforeRequest() {
        const url1 = getServerURL('echo');
        const url2 = getURLWebAccessible();
        const serverRedirect = getServerURL(`server-redirect?${url1}`);
        const listener = function(details) {
          return {redirectUrl: url2};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener,
            {urls: [url1]},
            ['blocking'],
        );

        assertRedirectSucceeds(serverRedirect, url2, function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function redirectToUnallowedServerRedirectOnBeforeRequest() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLNonWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        // The page should be redirected to the non web accessible URL, but this
        // URL will not load.
        assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function redirectToAboutUrlWithServerRedirect() {
        assertRedirectFails(getServerURL(`server-redirect?${aboutURL}`));
      },

      function redirectToDataUrlWithServerRedirect() {
        assertRedirectFails(getServerURL(`server-redirect?${dataURL}`));
      },

      function redirectToNonWebAccessibleUrlWithServerRedirect() {
        assertRedirectSucceeds(
            getServerURL(`server-redirect?${getURLNonWebAccessible()}`),
            getURLNonWebAccessible());
      },

      function redirectToWebAccessibleUrlWithServerRedirect() {
        assertRedirectSucceeds(
            getServerURL(`server-redirect?${getURLWebAccessible()}`),
            getURLWebAccessible());
      },

      function beforeRequestRedirectAfterServerRedirect() {
        const finalURL = getServerURL('echo?foo');
        const intermediateURL = getServerURL('echo?bar');
        const redirectURL = getServerURL(`server-redirect?${intermediateURL}`);

        const onBeforeSendHeadersListener = function(details) {
          chrome.test.assertFalse(
              details.url == intermediateURL,
              'intermediateURL should be redirected before the request starts.');
        };
        // Make sure all URLs use the extraHeaders path to expose
        // http://crbug.com/41433833.
        chrome.webRequest.onBeforeSendHeaders.addListener(
            onBeforeSendHeadersListener, {urls: ['<all_urls>']},
            ['blocking', 'extraHeaders']);

        const onBeforeRequestListener = function(details) {
          return {redirectUrl: finalURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            onBeforeRequestListener, {urls: [intermediateURL]}, ['blocking']);

        assertRedirectSucceeds(redirectURL, finalURL, function() {
          chrome.webRequest.onBeforeRequest.removeListener(
              onBeforeRequestListener);
          chrome.webRequest.onBeforeSendHeaders.removeListener(
              onBeforeSendHeadersListener);
        });
      },

      function serverRedirectChain() {
        const url = getServerURL('echo');
        const redirectURL = getServerURL(
            'server-redirect?' + getServerURL(`server-redirect?${url}`));
        const listener = function(details) {};
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: ['<all_urls>']}, onHeadersReceivedExtraInfoSpec);

        assertRedirectSucceeds(redirectURL, url, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function redirectHasSameRequestIdOnHeadersReceived() {
        const url = getServerURL('echo');
        let requestId;
        const onHeadersReceivedListener = function(details) {
          requestId = details.requestId;
          return {redirectUrl: getURLWebAccessible()};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            onHeadersReceivedListener, {urls: [url]},
            onHeadersReceivedExtraInfoSpec);

        const onBeforeRequestListener =
            chrome.test.callbackPass(function(details) {
              chrome.test.assertEq(details.requestId, requestId);
            });
        chrome.webRequest.onBeforeRequest.addListener(
            onBeforeRequestListener, {urls: [getURLWebAccessible()]});

        assertRedirectSucceeds(url, getURLWebAccessible(), function() {
          chrome.webRequest.onHeadersReceived.removeListener(
              onHeadersReceivedListener);
          chrome.webRequest.onBeforeRequest.removeListener(
              onBeforeRequestListener);
        });
      },

      function redirectHasSameRequestIdOnBeforeRequest() {
        const url = getServerURL('echo');
        let requestId;
        const onBeforeRequestRedirectListener = function(details) {
          requestId = details.requestId;
          return {redirectUrl: getURLWebAccessible()};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            onBeforeRequestRedirectListener, {urls: [url]}, ['blocking']);

        const onBeforeRequestListener =
            chrome.test.callbackPass(function(details) {
              chrome.test.assertEq(details.requestId, requestId);
            });
        chrome.webRequest.onBeforeRequest.addListener(
            onBeforeRequestListener, {urls: [getURLWebAccessible()]});

        assertRedirectSucceeds(url, getURLWebAccessible(), function() {
          chrome.webRequest.onBeforeRequest.removeListener(
              onBeforeRequestRedirectListener);
          chrome.webRequest.onBeforeRequest.removeListener(
              onBeforeRequestListener);
        });
      },
    ]);
  });
});
