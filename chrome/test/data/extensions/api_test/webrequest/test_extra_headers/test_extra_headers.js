// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function getSetCookieUrl(name, value) {
  return getServerURL('set-cookie?' + name + '=' + value);
}

function testModifyHeadersOnRedirect(useExtraHeaders) {
  // Use /echoheader instead of observing headers in onSendHeaders to
  // ensure we're looking at what the server receives. This avoids bugs in the
  // webRequest implementation from being masked.
  var finalURL = getServerURL('echoheader?User-Agent&Accept&X-New-Header');
  var url = getServerURL('server-redirect?' + finalURL);
  var listener = callbackPass(function(details) {
    var headers = details.requestHeaders;

    // Test modification.
    var accept_value;
    for (var i = 0; i < headers.length; i++) {
      if (headers[i].name.toLowerCase() === 'user-agent') {
        headers[i].value = 'foo';
      } else if (headers[i].name.toLowerCase() === 'accept') {
        accept_value = headers[i].value;
      }
    }

    // Test removal.
    chrome.test.assertTrue(accept_value.indexOf('image/webp') >= 0);
    removeHeader(headers, 'accept');

    // Test addition.
    headers.push({name: 'X-New-Header', value: 'Baz'});

    return {requestHeaders: headers};
  });

  var extraInfo = ['requestHeaders', 'blocking'];
  if (useExtraHeaders) {
    extraInfo.push('extraHeaders');
  }
  chrome.webRequest.onBeforeSendHeaders.addListener(listener,
      {urls: [finalURL]}, extraInfo);

  navigateAndWait(url, function(tab) {
    chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
    chrome.tabs.executeScript(tab.id, {
      code: 'document.body.innerText'
    }, callbackPass(function(results) {
      chrome.test.assertTrue(results[0].indexOf('foo') >= 0,
          'User-Agent should be modified.');
        chrome.test.assertTrue(results[0].indexOf('image/webp') == -1,
          'Accept should be removed.');
      chrome.test.assertTrue(results[0].indexOf('Baz') >= 0,
          'X-New-Header should be added.');
    }));
  });
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  function testSpecialRequestHeadersVisible() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echo');
      var extraHeadersListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders, ['user-agent', 'cookie'], []);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(extraHeadersListener,
          {urls: [url]}, ['requestHeaders', 'extraHeaders']);

      var standardListener = callbackPass(function(details) {
        checkHeaders(details.requestHeaders, ['user-agent'], ['cookie']);
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(standardListener,
          {urls: [url]}, ['requestHeaders']);

      navigateAndWait(url, function() {
        chrome.webRequest.onBeforeSendHeaders.removeListener(
            extraHeadersListener);
        chrome.webRequest.onBeforeSendHeaders.removeListener(standardListener);
      });
    });
  },

  function testSpecialResponseHeadersVisible() {
    var url = getSetCookieUrl('foo', 'bar');
    var extraHeadersListenerCalledCount = 0;
    function extraHeadersListener(details) {
      extraHeadersListenerCalledCount++;
      checkHeaders(details.responseHeaders, ['set-cookie'], []);
    }
    chrome.webRequest.onHeadersReceived.addListener(extraHeadersListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);
    chrome.webRequest.onResponseStarted.addListener(extraHeadersListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);
    chrome.webRequest.onCompleted.addListener(extraHeadersListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);

    var standardListenerCalledCount = 0;
    function standardListener(details) {
      standardListenerCalledCount++;
      checkHeaders(details.responseHeaders, [], ['set-cookie']);
    }
    chrome.webRequest.onHeadersReceived.addListener(standardListener,
        {urls: [url]}, ['responseHeaders']);
    chrome.webRequest.onResponseStarted.addListener(standardListener,
        {urls: [url]}, ['responseHeaders']);
    chrome.webRequest.onCompleted.addListener(standardListener,
        {urls: [url]}, ['responseHeaders']);

    navigateAndWait(url, function() {
      chrome.test.assertEq(3, standardListenerCalledCount);
      chrome.test.assertEq(3, extraHeadersListenerCalledCount);
      chrome.webRequest.onHeadersReceived.removeListener(extraHeadersListener);
      chrome.webRequest.onResponseStarted.removeListener(extraHeadersListener);
      chrome.webRequest.onCompleted.removeListener(extraHeadersListener);
      chrome.webRequest.onHeadersReceived.removeListener(standardListener);
      chrome.webRequest.onResponseStarted.removeListener(standardListener);
      chrome.webRequest.onCompleted.removeListener(standardListener);
    });
  },

  function testModifySpecialRequestHeaders() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echoheader?Cookie');
      var listener = callbackPass(function(details) {
        removeHeader(details.requestHeaders, 'cookie');
        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(listener,
          {urls: [url]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(tab.id, {
          code: 'document.body.innerText'
        }, callbackPass(function(results) {
          chrome.test.assertTrue(results[0].indexOf('bar') == -1,
              'Header not removed.');
        }));
      });
    });
  },

  function testModifySpecialResponseHeaders() {
    var url = getSetCookieUrl('foo', 'bar');
    var headersListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, ['set-cookie'], []);
      details.responseHeaders.push({name: 'X-New-Header',
                                    value: 'Foo'});
      return {responseHeaders: details.responseHeaders};
    });
    chrome.webRequest.onHeadersReceived.addListener(headersListener,
        {urls: [url]}, ['responseHeaders', 'blocking', 'extraHeaders']);

    var responseListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, ['set-cookie', 'x-new-header'], []);
    });
    chrome.webRequest.onResponseStarted.addListener(responseListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);

    var completedListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, ['set-cookie', 'x-new-header'], []);
    });
    chrome.webRequest.onCompleted.addListener(completedListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);

    navigateAndWait(url, function(tab) {
      chrome.webRequest.onHeadersReceived.removeListener(headersListener);
      chrome.webRequest.onResponseStarted.removeListener(responseListener);
      chrome.webRequest.onCompleted.removeListener(completedListener);
    });
  },

  function testCannotModifySpecialRequestHeadersWithoutExtraHeaders() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var url = getServerURL('echoheader?Cookie');
      var listener = callbackPass(function(details) {
        removeHeader(details.requestHeaders, 'cookie');
        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(listener,
          {urls: [url]}, ['requestHeaders', 'blocking']);

      // Add a no-op listener with extraHeaders to make sure it does not affect
      // the other listener.
      var noop = function() {};
      chrome.webRequest.onBeforeSendHeaders.addListener(noop,
          {urls: [url]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.webRequest.onBeforeSendHeaders.removeListener(noop);
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(tab.id, {
          code: 'document.body.innerText'
        }, callbackPass(function(results) {
          chrome.test.assertTrue(results[0].indexOf('bar') >= 0,
              'Header should not be removed.');
        }));
      });
    });
  },

  function testModifyUserAgentWithoutExtraHeaders() {
    var url = getServerURL('echoheader?User-Agent');
    var listener = callbackPass(function(details) {
      var headers = details.requestHeaders;
      for (var i = 0; i < headers.length; i++) {
        if (headers[i].name.toLowerCase() === 'user-agent') {
          headers[i].value = 'foo';
          break;
        }
      }
      return {requestHeaders: headers};
    });
    chrome.webRequest.onBeforeSendHeaders.addListener(listener,
        {urls: [url]}, ['requestHeaders', 'blocking']);

    navigateAndWait(url, function(tab) {
      chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
      chrome.tabs.executeScript(tab.id, {
        code: 'document.body.innerText'
      }, callbackPass(function(results) {
        chrome.test.assertTrue(results[0].indexOf('foo') >= 0,
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
    var url = getSetCookieUrl('theName', 'theValue');
    var listener = callbackPass(function(details) {
      removeHeader(details.responseHeaders, 'set-cookie');
      return {responseHeaders: details.responseHeaders};
    });
    chrome.webRequest.onHeadersReceived.addListener(listener,
        {urls: [url]}, ['responseHeaders', 'blocking']);

    // Add a no-op listener with extraHeaders to make sure it does not affect
    // the other listener.
    var noop = function() {};
    chrome.webRequest.onHeadersReceived.addListener(noop,
        {urls: [url]}, ['responseHeaders', 'blocking', 'extraHeaders']);

    navigateAndWait(url, function(tab) {
      chrome.webRequest.onHeadersReceived.removeListener(noop);
      chrome.webRequest.onHeadersReceived.removeListener(listener);
      chrome.tabs.executeScript(tab.id, {
        code: 'document.cookie'
      }, callbackPass(function(results) {
        chrome.test.assertTrue(results[0].indexOf('theValue') >= 0,
            'Header should not be removed.');
      }));
    });
  },

  function testRedirectToUrlWithExtraHeadersListener() {
    // Set a cookie so the cookie request header is set.
    navigateAndWait(getSetCookieUrl('foo', 'bar'), function() {
      var finalURL = getServerURL('echoheader?Cookie');
      var url = getServerURL('server-redirect?' + finalURL);
      var listener = callbackPass(function(details) {
        removeHeader(details.requestHeaders, 'cookie');
        return {requestHeaders: details.requestHeaders};
      });
      chrome.webRequest.onBeforeSendHeaders.addListener(listener,
          {urls: [finalURL]}, ['requestHeaders', 'blocking', 'extraHeaders']);

      navigateAndWait(url, function(tab) {
        chrome.test.assertEq(finalURL, tab.url);
        chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
        chrome.tabs.executeScript(tab.id, {
          code: 'document.body.innerText'
        }, callbackPass(function(results) {
          chrome.test.assertTrue(results[0].indexOf('bar') == -1,
              'Header not removed.');
        }));
      });
    });
  },
])});
