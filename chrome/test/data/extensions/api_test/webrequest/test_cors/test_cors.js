// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;
const listeningUrlPattern = '*://cors.example.com/*';
const params = (new URL(location.href)).searchParams;
const BASE = 'extensions/api_test/webrequest/cors/';

function setExpectationsForNonObservablePreflight() {
  // In this case the preflight request is not observable.
  const url = getServerURL(BASE + 'accept', 'cors.example.com');
  const method = 'GET';
  const initiator = getServerURL('').slice(0, -1);
  const type = 'xmlhttprequest';
  const frameUrl = 'unknown frame URL';
  const documentId = 1;

  expect(
      [  // events
        { label: 'onBeforeRequest',
          event: 'onBeforeRequest',
          details: {
            url,
            method,
            initiator,
            type,
            frameUrl,
            documentId,
          },
        },
        { label: 'onBeforeSendHeaders',
          event: 'onBeforeSendHeaders',
          details: {
            url,
            method,
            initiator,
            type,
            documentId,
          },
        },
        { label: 'onSendHeaders',
          event: 'onSendHeaders',
          details: {
            url,
            method,
            initiator,
            type,
            documentId,
          },
        },
        { // CORS fails due to lack of 'access-control-allow-headers' header.
          label: 'onErrorOccurred',
          event: 'onErrorOccurred',
          details: {
            url,
            method,
            error: 'net::ERR_FAILED',
            initiator,
            type,
            fromCache: false,
            documentId,
          }
        }
      ],
      [ // event order
        ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
         'onErrorOccurred']
      ],
      {urls: [url]},  // filter
      []  // extraInfoSpec
  );
}

function setExpectationsForObservablePreflight(extraInfoSpec) {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');
  const initiator = getServerURL('').slice(0, -1);
  const frameUrl = 'unknown frame URL';
  const type = 'xmlhttprequest';
  const documentId = 1;

  const eventsForPreflight = [
    { label: 'onBeforeRequest-P',
      event: 'onBeforeRequest',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        frameUrl,
        documentId,
      },
    },
    { label: 'onBeforeSendHeaders-P',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onSendHeaders-P',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onHeadersReceived-P',
      event: 'onHeadersReceived',
      details: {
        url,
        method: 'OPTIONS',
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onResponseStarted-P',
      event: 'onResponseStarted',
      details: {
        url,
        method: 'OPTIONS',
        ip: '127.0.0.1',
        fromCache: false,
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        documentId,
      },
    },
  ];
  const eventOrderForPreflight = [
    'onBeforeRequest-P', 'onBeforeSendHeaders-P', 'onSendHeaders-P',
    'onHeadersReceived-P', 'onResponseStarted-P',
  ];
  // The completion event of the preflight request coming from the network OR
  // The cancellation event of the preflight request coming from the CORS module
  // should arrive, but we are not sure which comes first - that is essentially
  // racy, so we cannot have an expecation here.

  // First, onBeforeRequest is called for the actual request, and then the
  // preflight request is made. As there is no 'access-control-allow-headers'
  // header in the preflight response, the actual request fails whereas the
  // preflight request succeeds.
  let events = [
    { label: 'onBeforeRequest',
      event: 'onBeforeRequest',
      details: {
        url: url,
        method: 'GET',
        initiator,
        type: 'xmlhttprequest',
        frameUrl: 'unknown frame URL',
        documentId: 1,
      },
    },
  ].concat(eventsForPreflight);
  let eventOrder = ['onBeforeRequest'].concat(eventOrderForPreflight);

  // We should see the cancellation of the actual request, but we cannot
  // have that expecation here because we don't have an expecation on
  // the completion of the preflight request. See above.

  expect(
      events,
      [eventOrder],
      {urls: [url]},  // filter
      extraInfoSpec,
  );
}

function registerOriginListeners(
    requiredNames, disallowedNames, extraInfoSpec) {
  let observed = false;
  const beforeSendHeadersListener = callbackPass(details => {
    observed = true;
    checkHeaders(details.requestHeaders, requiredNames, disallowedNames);
    chrome.webRequest.onBeforeSendHeaders.removeListener(
        beforeSendHeadersListener);
  });
  chrome.webRequest.onBeforeSendHeaders.addListener(
      beforeSendHeadersListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedListener = callbackPass(() => {
    chrome.test.assertTrue(observed);
    chrome.webRequest.onCompleted.removeListener(onCompletedListener);
  });
  chrome.webRequest.onCompleted.addListener(
      onCompletedListener, {urls: [listeningUrlPattern]});
}

function registerRequestHeaderInjectionListeners(extraInfoSpec) {
  const beforeSendHeadersListener = callbackPass(details => {
    details.requestHeaders.push({name: 'x-foo', value: 'trigger-preflight'});
    return {requestHeaders: details.requestHeaders};
  });
  chrome.webRequest.onBeforeSendHeaders.addListener(
      beforeSendHeadersListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // If the 'x-foo' header is injected by |beforeSendHeadersListener| without
  // 'extraHeaders', it triggers CORS preflight, and the response for the
  // preflight OPTIONS request is expected to have the
  // 'Access-Control-Allow-Headers: x-foo' header to pass the security checks.
  // Since the mock-http-headers for the target URL does not provide the
  // required header, the request fails in the CORS preflight. Otherwises,
  // modified headers are not observed by CORS implementations, and do not
  // trigger the CORS preflight.
  const triggerPreflight = !extraInfoSpec.includes('extraHeaders');

  const event = triggerPreflight ? chrome.webRequest.onErrorOccurred :
                                   chrome.webRequest.onCompleted;

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedOrErrorOccurredListener = callbackPass(details => {
    chrome.webRequest.onBeforeSendHeaders.removeListener(
        beforeSendHeadersListener);
    event.removeListener(onCompletedOrErrorOccurredListener);
  });
  event.addListener(
      onCompletedOrErrorOccurredListener, {urls: [listeningUrlPattern]});
}
function registerResponseHeaderInjectionListeners(extraInfoSpec) {
  const headersReceivedListener = details => {
    details.responseHeaders.push(
        {name: 'Access-Control-Allow-Origin', value: '*'});
    return { responseHeaders: details.responseHeaders };
  };
  chrome.webRequest.onHeadersReceived.addListener(
      headersReceivedListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // If the 'extraHeaders' is not specified, Chrome detects CORS failures
  // before |headerReceivedListener| is called and injects fake headers to
  // deceive the CORS checks.
  const canInjectFakeCorsResponse = extraInfoSpec.includes('extraHeaders');

  const event = canInjectFakeCorsResponse ? chrome.webRequest.onCompleted :
                                            chrome.webRequest.onErrorOccurred;

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedOrErrorOccurredListener = callbackPass(details => {
    chrome.webRequest.onHeadersReceived.removeListener(headersReceivedListener);
    event.removeListener(onCompletedOrErrorOccurredListener);
  });
  event.addListener(
      onCompletedOrErrorOccurredListener, {urls: [listeningUrlPattern]});
}

function setExpectationsForSuccessfulPreflight() {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');
  const initiator = getServerURL('').slice(0, -1);
  const frameUrl = 'unknown frame URL';
  const type = 'xmlhttprequest';
  const documentId = 1;

  const events = [
    { label: 'onBeforeRequest-P',
      event: 'onBeforeRequest',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        frameUrl,
        documentId,
      },
    },
    { label: 'onBeforeSendHeaders-P',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onSendHeaders-P',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onHeadersReceived-P',
      event: 'onHeadersReceived',
      details: {
        url,
        method: 'OPTIONS',
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
      retval_function: (name, details) => {
        // Allow the 'x-foo' header, so that the preflight succeeds.
        details.responseHeaders.push(
            {name: 'access-control-allow-headers', value: 'x-foo'});
        // Prevent the CORS preflight cache from reusing this preflight
        // response.
        details.responseHeaders.push(
            {name: 'access-control-max-age', value: '0'});
        return {responseHeaders: details.responseHeaders};
      },
    },
    { label: 'onResponseStarted-P',
      event: 'onResponseStarted',
      details: {
        url,
        method: 'OPTIONS',
        ip: '127.0.0.1',
        fromCache: false,
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
    },
    { label: 'onCompleted-P',
      event: 'onCompleted',
      details: {
        url,
        method: 'OPTIONS',
        ip: '127.0.0.1',
        fromCache: false,
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
    },
    { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          url: url,
          method: 'GET',
          initiator,
          type: 'xmlhttprequest',
          frameUrl: 'unknown frame URL',
          documentId,
        },
      },
    { label: 'onBeforeSendHeaders',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'GET',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onSendHeaders',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'GET',
        initiator,
        type,
        documentId,
      },
    },
    { label: 'onHeadersReceived',
      event: 'onHeadersReceived',
      details: {
        url,
        method: 'GET',
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
    },
    { label: 'onResponseStarted',
      event: 'onResponseStarted',
      details: {
        url,
        method: 'GET',
        ip: '127.0.0.1',
        fromCache: false,
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
    },
    { label: 'onCompleted',
      event: 'onCompleted',
      details: {
        url,
        method: 'GET',
        ip: '127.0.0.1',
        fromCache: false,
        statusCode: 200,
        statusLine: 'HTTP/1.1 200 OK',
        initiator,
        type,
        responseHeadersExist: true,
        documentId,
      },
    },
  ];
  let eventOrder = [
    'onBeforeRequest',
    'onBeforeRequest-P',
    'onBeforeSendHeaders-P',
    'onSendHeaders-P',
    'onHeadersReceived-P',
    'onResponseStarted-P',
    'onCompleted-P',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onResponseStarted',
    'onCompleted',
  ];
  expect(
      events,
      [eventOrder],
      {urls: [url]},  // filter
      ['blocking', 'responseHeaders', 'extraHeaders'] // extraInfoSpec
  );
}

function registerPreflightBlockingListener() {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');

  const onHeadersReceivedCalledForPreflight = callbackPass(() => {});
  chrome.webRequest.onHeadersReceived.addListener(
      function onHeadersReceived(details) {
        if (details.method === 'OPTIONS') {
          onHeadersReceivedCalledForPreflight();
          // Synchronously removing the listener breaks the behavior.
          setTimeout(() => {
            chrome.webRequest.onHeadersReceived.removeListener(
                onHeadersReceived)
          }, 0);
          return {cancel: true};
        }
      }, {urls: [url]}, ['blocking', 'extraHeaders']);

  const done = callbackPass(() => {});
  let hasSeenPreflightError = false;
  chrome.webRequest.onErrorOccurred.addListener(
      function onErrorOccurred(details) {
        if (details.method === 'OPTIONS') {
          hasSeenPreflightError = true;
        }

        if (details.method === 'GET') {
          chrome.webRequest.onErrorOccurred.removeListener(onErrorOccurred);
          chrome.test.assertTrue(hasSeenPreflightError);
          done();
        }
      }, {urls: [url]});
}

function registerPreflightRedirectingListener() {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');

  const onBeforeRequestCalledForPreflight = callbackPass(() => {});
  chrome.webRequest.onBeforeRequest.addListener(
      function onBeforeRequest(details) {
        if (details.method === 'OPTIONS') {
          onBeforeRequestCalledForPreflight();
          // Synchronously removing the listener breaks the behavior.
          setTimeout(() => {
            chrome.webRequest.onBeforeRequest.removeListener(onBeforeRequest)
          }, 0);
          return {redirectUrl: url + '?redirected'};
        }
      }, {urls: [url]}, ['blocking', 'extraHeaders']);

  // We see failures on both the preflight and the actual request.
  const done = callbackPass(() => {});
  let hasSeenPreflightError = false;
  chrome.webRequest.onErrorOccurred.addListener(
      function onErrorOccurred(details) {
        if (details.method === 'OPTIONS') {
          hasSeenPreflightError = true;
        }

        if (details.method === 'GET') {
          chrome.webRequest.onErrorOccurred.removeListener(onErrorOccurred);
          chrome.test.assertTrue(hasSeenPreflightError);
          done();
        }
      }, {urls: [url]});
}

function registerOnBeforeRequestAndOnErrorOcurredListeners() {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');

  const onBeforeRequestCalledForPreflight = callbackPass(() => {});
  // onBeforeRequest doesn't have "extraHeaders", but it sees a preflight
  // because onErrorOccurred has "extraHeaders".
  chrome.webRequest.onBeforeRequest.addListener((details) => {
    if (details.method === 'OPTIONS') {
      onBeforeRequestCalledForPreflight();
    }
  }, {urls: [url]});

  chrome.webRequest.onErrorOccurred.addListener(() => {
  }, {urls: [url]}, ['extraHeaders']);
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  function testOriginHeader() {
    // Register two sets of listener. One with extraHeaders and the second one
    // without it. The Origin header is invisible if the extraHeaders is not
    // specified.
    registerOriginListeners([], ['origin'], ['requestHeaders']);
    registerOriginListeners(['origin'], [], ['requestHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsSensitiveHeaderInjectionWithoutExtraHeaders() {
    registerRequestHeaderInjectionListeners(['blocking', 'requestHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsSensitiveHeaderInjectionWithExtraHeaders() {
    registerRequestHeaderInjectionListeners(
        ['blocking', 'requestHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsResponseHeaderInjectionWithoutExtraHeaders() {
    registerResponseHeaderInjectionListeners(
        ['blocking', 'responseHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=reject'));
  },
  function testCorsResponseHeaderInjectionWithExtraHeaders() {
    registerResponseHeaderInjectionListeners(
        ['blocking', 'responseHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=reject'));
  },
  function testCorsPreflightWithoutExtraHeaders() {
    setExpectationsForNonObservablePreflight();
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsPreflightWithExtraHeaders() {
    setExpectationsForObservablePreflight(['extraHeaders']);
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsPreflightModificationWithExtraHeaders() {
    setExpectationsForSuccessfulPreflight();
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsPreflightBlockIsBlocked() {
    registerPreflightBlockingListener();
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsPreflightRedirect() {
    registerPreflightRedirectingListener();
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsPreflightIsObservableWhenAnyListenerHasExtraHeaders() {
    registerOnBeforeRequestAndOnErrorOcurredListeners();
    navigateAndWait(getServerURL(
        BASE + 'fetch.html?path=accept&with-preflight'));
  },
  function testCorsServerRedirect() {
    const url = getServerURL('server-redirect?whatever', 'cors.example.com');

    const callback = callbackPass(() => {});
    chrome.webRequest.onHeadersReceived.addListener((details) => {
      if (details.url === url && details.method === 'GET') {
        callback();
      }
    }, {urls: ["http://*/*"]}, ['extraHeaders']);

    const absPath =
          encodeURIComponent(`/server-redirect?${encodeURIComponent(url)}`);
    navigateAndWait(getServerURL(
        BASE + `fetch.html?abspath=${absPath}&with-preflight`));
  },
])});
