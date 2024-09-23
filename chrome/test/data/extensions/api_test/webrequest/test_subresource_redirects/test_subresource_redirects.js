// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const isServiceWorker = ('ServiceWorkerGlobalScope' in self);

var pass = chrome.test.callbackPass;
var dataURL = 'data:text/plain,redirected1';

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
      if (callback) callback();
      chrome.test.assertEq(response.url, redirectURL);
    });
    fetch(url).then((response) => {
      passCallback(response);
    }).catch((e) => {
      if (callback) callback();
      chrome.test.fail(e);
    });
  });
}

function assertRedirectFails(url, callback) {
  // Load a page to be sure webRequest listeners are set up.
  navigateAndWait(getURL('simpleLoad/b.html'), function() {
    passCallback = pass(() => {
      if (callback) callback();
    });
    fetch(url).then((response) => {
      if (callback) callback();
      chrome.test.fail();
    }).catch((e) => {
      passCallback();
    });
  });
}

const nonServiceWorkerTests = [
  // TODO(crbug.com/40243056): These two tests hang.
  'subresourceRedirectHasSameRequestIdOnHeadersReceived',
  'subresourceRedirectHasSameRequestIdOnBeforeRequest'
];

function getFilteredTests(tests) {
  if (!isServiceWorker) {
    return tests;
  }
  return tests.filter(function(op) {
    return !nonServiceWorkerTests.includes(op.name);
  });
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.test.getConfig(function(config) {
  var onHeadersReceivedExtraInfoSpec = ['blocking'];
  if (config.customArg) {
    let args = JSON.parse(config.customArg);
    if (args.useExtraHeaders) {
      onHeadersReceivedExtraInfoSpec.push('extraHeaders');
    }
  }

  runTests(getFilteredTests([
    function subresourceRedirectToDataUrlOnHeadersReceived() {
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

    function subresourceRedirectToNonWebAccessibleUrlOnHeadersReceived() {
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

    function subresourceRedirectToServerRedirectOnHeadersReceived() {
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

    function subresourceRedirectToUnallowedServerRedirectOnHeadersReceived() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onHeadersReceived.addListener(listener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      assertRedirectFails(url, function() {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
      });
    },

    function subresourceRedirectToDataUrlOnBeforeRequest() {
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

    function subresourceRedirectToNonWebAccessibleUrlOnBeforeRequest() {
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

    function subresourceRedirectToServerRedirectOnBeforeRequest() {
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

    function subresourceRedirectToUnallowedServerRedirectOnBeforeRequest() {
      var url = getServerURL('echo');
      var redirectURL = getServerURL('server-redirect?' +
          getURLNonWebAccessible());
      var listener = function(details) {
        return {redirectUrl: redirectURL};
      };
      chrome.webRequest.onBeforeRequest.addListener(listener,
          {urls: [url]}, ['blocking']);

      assertRedirectFails(url, function() {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
      });
    },

    function subresourceRedirectToDataUrlWithServerRedirect() {
      assertRedirectFails(getServerURL('server-redirect?' + dataURL));
    },

    function subresourceRedirectToNonWebAccessibleWithServerRedirect() {
      assertRedirectFails(
          getServerURL('server-redirect?' + getURLNonWebAccessible()));
    },

    function subresourceRedirectToWebAccessibleWithServerRedirect() {
      assertRedirectSucceeds(
          getServerURL('server-redirect?' + getURLWebAccessible()),
          getURLWebAccessible());
    },

    function subresourceRedirectHasSameRequestIdOnHeadersReceived() {
      var url = getServerURL('echo');
      var requestId;
      var onHeadersReceivedListener = function(details) {
        requestId = details.requestId;
        return {redirectUrl: getURLWebAccessible()};
      };
      chrome.webRequest.onHeadersReceived.addListener(onHeadersReceivedListener,
          {urls: [url]}, onHeadersReceivedExtraInfoSpec);

      var onBeforeRequestListener = chrome.test.callbackPass(function(details) {
        chrome.test.assertEq(details.requestId, requestId);
      });
      chrome.webRequest.onBeforeRequest.addListener(onBeforeRequestListener,
          {urls: [getURLWebAccessible()]});

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onHeadersReceived.removeListener(
            onHeadersReceivedListener);
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestListener);
      });
    },

    function subresourceRedirectHasSameRequestIdOnBeforeRequest() {
      var url = getServerURL('echo');
      var requestId;
      var onBeforeRequestRedirectListener = function(details) {
        requestId = details.requestId;
        return {redirectUrl: getURLWebAccessible()};
      };
      chrome.webRequest.onBeforeRequest.addListener(
          onBeforeRequestRedirectListener, {urls: [url]}, ['blocking']);

      var onBeforeRequestListener = chrome.test.callbackPass(function(details) {
        chrome.test.assertEq(details.requestId, requestId);
      });
      chrome.webRequest.onBeforeRequest.addListener(onBeforeRequestListener,
          {urls: [getURLWebAccessible()]});

      assertRedirectSucceeds(url, getURLWebAccessible(), function() {
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestRedirectListener);
        chrome.webRequest.onBeforeRequest.removeListener(
            onBeforeRequestListener);
      });
    },
  ]));
})});
