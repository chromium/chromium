// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const isServiceWorker = ('ServiceWorkerGlobalScope' in self);

const pass = chrome.test.callbackPass;
const dataURL = 'data:text/plain,redirected1';

function getURLNonWebAccessible() {
  return getURL('manifest.json');
}

function getURLWebAccessible() {
  return getURL('simpleLoad/a.html');
}

function assertRedirectSucceeds(url, redirectURL, callback) {
  // Load a page to be sure webRequest listeners are set up.
  navigateAndWait(getURL('simpleLoad/b.html'), function() {
    passCallback = pass((response) => {
      if (callback) {
        callback();
      }
      chrome.test.assertEq(response.url, redirectURL);
    });
    fetch(url)
        .then((response) => {
          passCallback(response);
        })
        .catch((e) => {
          if (callback) {
            callback();
          }
          chrome.test.fail(e);
        });
  });
}

function assertRedirectFails(url, callback) {
  // Load a page to be sure webRequest listeners are set up.
  navigateAndWait(getURL('simpleLoad/b.html'), function() {
    passCallback = pass(() => {
      if (callback) {
        callback();
      }
    });
    fetch(url)
        .then((response) => {
          if (callback) {
            callback();
          }
          chrome.test.fail();
        })
        .catch((e) => {
          passCallback();
        });
  });
}

const nonServiceWorkerTests = [
  // TODO(crbug.com/40243056): These two tests hang.
  'subresourceRedirectHasSameRequestIdOnHeadersReceived',
  'subresourceRedirectHasSameRequestIdOnBeforeRequest',
];

function getFilteredTests(tests) {
  if (!isServiceWorker) {
    return tests;
  }
  return tests.filter(function(op) {
    return !nonServiceWorkerTests.includes(op.name);
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

    runTests(getFilteredTests([
      function subresourceRedirectToDataUrlOnHeadersReceived() {
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

      function subresourceRedirectToNonWebAccessibleUrlOnHeadersReceived() {
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

      function subresourceRedirectToServerRedirectOnHeadersReceived() {
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

      function subresourceRedirectToUnallowedServerRedirectOnHeadersReceived() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLNonWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: [url]}, onHeadersReceivedExtraInfoSpec);

        assertRedirectFails(url, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },

      function subresourceRedirectToDataUrlOnBeforeRequest() {
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

      function subresourceRedirectToNonWebAccessibleUrlOnBeforeRequest() {
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

      function subresourceRedirectToServerRedirectOnBeforeRequest() {
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

      function subresourceRedirectToUnallowedServerRedirectOnBeforeRequest() {
        const url = getServerURL('echo');
        const redirectURL =
            getServerURL('server-redirect?' + getURLNonWebAccessible());
        const listener = function(details) {
          return {redirectUrl: redirectURL};
        };
        chrome.webRequest.onBeforeRequest.addListener(
            listener, {urls: [url]}, ['blocking']);

        assertRedirectFails(url, function() {
          chrome.webRequest.onBeforeRequest.removeListener(listener);
        });
      },

      function subresourceRedirectToDataUrlWithServerRedirect() {
        assertRedirectFails(getServerURL(`server-redirect?${dataURL}`));
      },

      function subresourceRedirectToNonWebAccessibleWithServerRedirect() {
        assertRedirectFails(
            getServerURL(`server-redirect?${getURLNonWebAccessible()}`));
      },

      function subresourceRedirectToWebAccessibleWithServerRedirect() {
        assertRedirectSucceeds(
            getServerURL(`server-redirect?${getURLWebAccessible()}`),
            getURLWebAccessible());
      },

      function subresourceRedirectHasSameRequestIdOnHeadersReceived() {
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

      function subresourceRedirectHasSameRequestIdOnBeforeRequest() {
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
    ]));
  });
});
