// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var dataURL = 'data:text/plain,redirected1';
var aboutURL = 'about:blank';

function getURLNonWebAccessible() {
  return getURL('manifest.json');
}

function getURLWebAccessible() {
  return getURL('simpleLoad/a.html');
}

function assertRedirectSucceeds(url, redirectURL, callback) {
  navigateAndWait(url, function(tab) {
    if (callback) callback();
    chrome.test.assertEq(redirectURL, tab.url);
  });
}

function assertRedirectFails(url) {
  navigateAndWait(url, function(tab) {
    // Tab URL should still be set to the original URL.
    chrome.test.assertEq(url, tab.url);
  });
}

chrome.test.getConfig(function(config) {
  var onHeadersReceivedExtraInfoSpec = ['blocking'];
  if (config.customArg === 'useExtraHeaders')
    onHeadersReceivedExtraInfoSpec.push('extraHeaders');

  runTests([
    function redirectToDataUrlOnHeadersReceived() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: dataURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, dataURL, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function redirectToAboutUrlOnHeadersReceived() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: aboutURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, aboutURL, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function redirectToNonWebAccessibleUrlOnHeadersReceived() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: getURLNonWebAccessible()};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function redirectToServerRedirectOnHeadersReceived() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function serverRedirectThenExtensionRedirectOnHeadersReceived() {
      var url_1 = getServerURL('echo');
      var url_2 = getURLWebAccessible();
      var serverRedirect = getServerURL('server-redirect?' + url_1);
      var listener = function(details) {
        return {redirectUrl: url_2};
      };
      chrome.webRequest.onHeadersReceived.addListener(
        listener,
        { urls: [url_1] },
        ["blocking"]
      );

      assertRedirectSucceeds(serverRedirect, url_2, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function redirectToUnallowedServerRedirectOnHeadersReceived() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      // The page should be redirected to the non web accessible URL, but this
      // URL will not load.
      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function redirectToDataUrlOnBeforeRequest() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: dataURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, dataURL, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function redirectToAboutUrlOnBeforeRequest() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: aboutURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, aboutURL, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function redirectToNonWebAccessibleUrlOnBeforeRequest() {
      var url = getServerURL('echo');
      var listener = function(details) {
        return {redirectUrl: getURLNonWebAccessible()};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function redirectToServerRedirectOnBeforeRequest() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    // A server redirect immediately followed by an extension redirect.
    // Regression test for:
    // - https://crbug.com/882661
    // - https://crbug.com/880741
    function serverRedirectThenExtensionRedirectOnBeforeRequest() {
      var url_1 = getServerURL('echo');
      var url_2 = getURLWebAccessible();
      var serverRedirect = getServerURL('server-redirect?' + url_1);
      var listener = function(details) {
        return {redirectUrl: url_2};
      };
      chrome.webRequest.onBeforeRequest.addListener(
        listener,
        { urls: [url_1] },
        ["blocking"]
      );

      assertRedirectSucceeds(serverRedirect, url_2, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function redirectToUnallowedServerRedirectOnBeforeRequest() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      // The page should be redirected to the non web accessible URL, but this
      // URL will not load.
      assertRedirectSucceeds(url, getURLNonWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function redirectToAboutUrlWithServerRedirect() {
      assertRedirectFails(getServerURL('server-redirect?' + aboutURL));
    },

    function redirectToDataUrlWithServerRedirect() {
      assertRedirectFails(getServerURL('server-redirect?' + dataURL));
    },

    function redirectToNonWebAccessibleUrlWithServerRedirect() {
      assertRedirectSucceeds(
          getServerURL('server-redirect?' + getURLNonWebAccessible()),
          getURLNonWebAccessible());
    },

    function redirectToWebAccessibleUrlWithServerRedirect() {
      assertRedirectSucceeds(
          getServerURL('server-redirect?' + getURLWebAccessible()),
          getURLWebAccessible());
    },

    function beforeRequestRedirectAfterServerRedirect() {
      var finalURL = getServerURL('echo?foo');
      var intermediateURL = getServerURL('echo?bar');
      var redirectURL = getServerURL('server-redirect?' + intermediateURL);

      var onBeforeSendHeadersListener = function(details) {
        chrome.test.assertFalse(details.url == intermediateURL,
            'intermediateURL should be redirected before the request starts.');
      };
      // Make sure all URLs use the extraHeaders path to expose
      // http://crbug.com/918761.
      chrome.webRequest.onBeforeSendHeaders.addListener(
          onBeforeSendHeadersListener,
          {urls: ['<all_urls>']}, ['blocking', 'extraHeaders']);

      var onBeforeRequestListener = function(details) {
        return {redirectUrl: finalURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(onBeforeRequestListener,
          {urls: [intermediateURL]}, ['blocking']);

      assertRedirectSucceeds(redirectURL, finalURL, function() {
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestListener);
        chrome.webRequest.onBeforeSendHeaders.removeListener(
            onBeforeSendHeadersListener);
      });
    },

    function serverRedirectChain() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getServerURL('server-redirect?' + url));
      var listener = function(details) {};
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: ['<all_urls>']}, onHeadersReceivedExtraInfoSpec);

      assertRedirectSucceeds(redirectURL, url, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },
  ]);
});
