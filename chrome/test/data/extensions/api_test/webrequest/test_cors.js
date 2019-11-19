// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;
const listeningUrlPattern = '*://cors.example.com/*';
const params = (new URL(location.href)).searchParams;
const BASE = 'extensions/api_test/webrequest/cors/';

function getCorsMode() {
  const name = 'cors_mode';
  chrome.test.assertTrue(params.has(name));
  const mode = params.get(name);
  chrome.test.assertTrue(mode == 'blink' || mode == 'network_service');
  return mode;
}

function isExtraHeadersForced() {
  return params.has('with_force_extra_headers');
}

function setExpectationsForNonObservablePreflight() {
  // In this case the preflight request is not observable.
  chrome.test.assertTrue(getCorsMode() == 'network_service');

  const url = getServerURL(BASE + 'accept', 'cors.example.com');
  const method = 'GET';
  const initiator = getServerURL('').slice(0, -1);
  const type = 'xmlhttprequest';
  const frameUrl = 'unknown frame URL';

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
          },
        },
        { label: 'onBeforeSendHeaders',
          event: 'onBeforeSendHeaders',
          details: {
            url,
            method,
            initiator,
            type,
          },
        },
        { label: 'onSendHeaders',
          event: 'onSendHeaders',
          details: {
            url,
            method,
            initiator,
            type,
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

  const eventsForPreflight = [
    { label: 'onBeforeRequest-P',
      event: 'onBeforeRequest',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        frameUrl,
      },
    },
    { label: 'onBeforeSendHeaders-P',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
      },
    },
    { label: 'onSendHeaders-P',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
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
      },
    },
  ];
  const eventOrderForPreflight = [
    'onBeforeRequest-P', 'onBeforeSendHeaders-P', 'onSendHeaders-P',
    'onHeadersReceived-P', 'onResponseStarted-P', 'onCompleted-P',
  ];

  let events;
  let eventsOrder;
  if (getCorsMode() == 'network_service') {
    // When the CORS module is in the network process, onBeforeRequest is called
    // for the actual request first, and then the preflight request is made.
    // As there is no 'access-control-allow-headers' header in the preflight
    // response, the actual request fails whereas the preflight request
    // succeeds.
    events = [
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          url: url,
          method: 'GET',
          initiator,
          type: 'xmlhttprequest',
          frameUrl: 'unknown frame URL',
        },
      },
    ].concat(eventsForPreflight, [
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          url: url,
          method: 'GET',
          error: 'net::ERR_FAILED',
          initiator: initiator,
          type: 'xmlhttprequest',
          fromCache: false,
        }
      },
    ]);
    eventOrder = ['onBeforeRequest'].concat(
        eventOrderForPreflight, ['onErrorOccurred']);
  } else {
    // In this case, the preflight request is made first, and blink will not
    // make the actual request because of the lack of an
    // 'access-control-allow-headers' header in the preflight response.
    events = eventsForPreflight;
    eventOrder = eventOrderForPreflight;
  }

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
  // 'extraHeaders' and with OOR-CORS being enabled, it triggers CORS
  // preflight, and the response for the preflight OPTIONS request is expected
  // to have the 'Access-Control-Allow-Headers: x-foo' header to pass the
  // security checks. Since the mock-http-headers for the target URL does not
  // provide the required header, the request fails in the CORS preflight.
  // Otherwises, modified headers are not observed by CORS implementations, and
  // do not trigger the CORS preflight.
  const triggerPreflight = !extraInfoSpec.includes('extraHeaders') &&
      !isExtraHeadersForced() && getCorsMode() == 'network_service';

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

  // If the 'extraHeaders' is not specified and OOR-CORS is enabled, Chrome
  // detects CORS failures before |headerReceivedListener| is called and injects
  // fake headers to deceive the CORS checks.
  const canInjectFakeCorsResponse = extraInfoSpec.includes('extraHeaders') ||
      isExtraHeadersForced() || getCorsMode() == 'blink';

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

  const events = [
    { label: 'onBeforeRequest-P',
      event: 'onBeforeRequest',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
        frameUrl,
      },
    },
    { label: 'onBeforeSendHeaders-P',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
      },
    },
    { label: 'onSendHeaders-P',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'OPTIONS',
        initiator,
        type,
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
        },
      },
    { label: 'onBeforeSendHeaders',
      event: 'onBeforeSendHeaders',
      details: {
        url,
        method: 'GET',
        initiator,
        type,
      },
    },
    { label: 'onSendHeaders',
      event: 'onSendHeaders',
      details: {
        url,
        method: 'GET',
        initiator,
        type,
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
      },
    },
  ];
  let eventOrder;
  if (getCorsMode() == 'network_service') {
    eventOrder = [
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
  } else {
    eventOrder = [
      'onBeforeRequest-P',
      'onBeforeSendHeaders-P',
      'onSendHeaders-P',
      'onHeadersReceived-P',
      'onResponseStarted-P',
      'onCompleted-P',
      'onBeforeRequest',
      'onBeforeSendHeaders',
      'onSendHeaders',
      'onHeadersReceived',
      'onResponseStarted',
      'onCompleted',
    ];
  }
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

        // We see an error event for the actual request only when OOR-CORS
        // is enabled; otherwise the CORS module in blink doesn't make a network
        // request for the actual request.
        if (details.method === 'GET' || getCorsMode() == 'blink') {
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

  if (getCorsMode() == 'network_service') {
    // When CORS is implemented in the network service, we see failures on both
    // the preflight and the actual request.
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
  } else {
    // In this case we see no completion events nor error events - The renderer
    // cancels the preflight request in the redirect handling logic, and
    // WebRequestProxyingURLLoaderFactory suppresses events in such a case.
    // See https://crbug.com/1014816.
  }
}

function registerOnBeforeRequestAndOnErrorOcurredListeners() {
  const url = getServerURL(BASE + 'accept', 'cors.example.com');

  const onBeforeRequestCalledForPreflight = callbackPass(() => {});
  // onBeforeRequest doesn't have "extraHeaders", but it sees a preflight
  // even when OOR-CORS is enabled, because onErrorOccurred has "extraHeaders".
  chrome.webRequest.onBeforeRequest.addListener((details) => {
    if (details.method === 'OPTIONS') {
      onBeforeRequestCalledForPreflight();
    }
  }, {urls: [url]});

  chrome.webRequest.onErrorOccurred.addListener(() => {
  }, {urls: [url]}, ['extraHeaders']);
}


runTests([
  function testOriginHeader() {
    // Register two sets of listener. One with extraHeaders and the second one
    // without it.
    // If OOR-CORS is enabled, the Origin header is invisible if the
    // extraHeaders is not specified.
    if (getCorsMode() == 'network_service' && !isExtraHeadersForced())
      registerOriginListeners([], ['origin'], ['requestHeaders']);
    else
      registerOriginListeners(['origin'], [], ['requestHeaders']);
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
    if (getCorsMode() == 'network_service' && !isExtraHeadersForced()) {
      setExpectationsForNonObservablePreflight();
    } else {
      setExpectationsForObservablePreflight([]);
    }
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
  }
]);
