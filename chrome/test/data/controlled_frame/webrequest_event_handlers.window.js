// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// This test expects WebRequest events to be triggered for a basic GET request
// inside Controlled Frame. 'onAuthRequired', 'onBeforeRedirect',
// 'onErrorOccurred' are expected to not trigger.

// The list of WebRequest events.
const WebRequestEvents = [
  'onBeforeRequest',
  'onBeforeSendHeaders',
  'onSendHeaders',
  'onAuthRequired',
  'onBeforeRedirect',
  'onHeadersReceived',
  'onResponseStarted',
  'onCompleted',
];

function addListeners(controlledframe, targetUrl) {
  window.events = [];

  const filter = {
    urls: [targetUrl],
    types: ['main_frame', 'xmlhttprequest'],
  };
  for (const eventName of WebRequestEvents) {
    const eventCounter = function(details) {
      window.events.push(eventName);
    };

    controlledframe.request[eventName].addListener(eventCounter, filter);
  }

  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  window.occurredErrors = [];
  controlledframe.request.onErrorOccurred.addListener(function(details) {
    window.events.push('onErrorOccurred');
    window.occurredErrors.push(details.error);
  }, filter);
}

function verifyEvents(expected) {
  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  if (window.occurredErrors.length > 0) {
    console.log(`onErrorOccurred triggered ${
        window.occurredErrors.length} times, errors are:\n${
        window.occurredErrors
            .map((item, index) => `${index + 1}. ${item}`)
            .join('\n')}\n`);
  }

  assert_equals(JSON.stringify(window.events), JSON.stringify(expected));
}

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/handbag.png';
  addListeners(controlledframe, targetUrl.toString());

  const script = `(async() => {
      const response = await fetch('${targetUrl.toString()}', {method:'GET'});
      await response.blob();
      })();`;
  await executeAsyncScript(controlledframe, script);

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onResponseStarted',
    'onCompleted',
  ]);
}, 'WebRequest Normal Request');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  targetUrl.pathname = '/server-redirect';
  targetUrl.search = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onBeforeRedirect',
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onResponseStarted',
    'onCompleted',
  ]);
}, 'WebRequest Redirect');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth1');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onAuthRequired.addListener(function(details) {
    return {
      authCredentials: {
        username: '',
        password: 'PASS',
      }
    };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/auth-basic';
  targetUrl.search = 'password=PASS&realm=REALM';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onAuthRequired',
    'onResponseStarted',
    'onCompleted',
  ]);
}, 'WebRequest Auth');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth2');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onAuthRequired.addListener(function(details, cb) {
    setTimeout(cb, 1, {
      authCredentials: {
        username: '',
        password: 'PASS',
      }
    });
  }, { urls: [targetUrl.toString()] }, ['asyncBlocking']);

  targetUrl.pathname = '/auth-basic';
  targetUrl.search = 'password=PASS&realm=REALM';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onAuthRequired',
    'onResponseStarted',
    'onCompleted',
  ]);
}, 'WebRequest Auth Async');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth3');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onAuthRequired.addListener(function(details) {
    return {
      authCredentials: {
        username: '',
        password: 'WRONG_PASSWORD',
      }
    };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/auth-basic';
  targetUrl.search = 'password=PASS&realm=REALM';
  try {
    await navigateControlledFrame(controlledframe, targetUrl.toString());
  } catch (e) {}

  assert_false(window.events.includes('onCompleted'));
  assert_true(window.events.includes('onAuthRequired'));
  assert_equals(window.events.slice(-1)[0], 'onErrorOccurred');
  assert_equals(window.occurredErrors[0], 'net::ERR_TOO_MANY_RETRIES');
}, 'WebRequest Auth Error');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onBeforeRequest.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyEvents(['onBeforeRequest', 'onErrorOccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeRequest');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onBeforeSendHeaders.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyEvents(['onBeforeRequest', 'onBeforeSendHeaders', 'onErrorOccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeSendHeaders');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onHeadersReceived.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onErrorOccurred'
  ]);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel headersReceived');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onBeforeSendHeaders.addListener(function(details) {
    return {
      requestHeaders: [
        { name: 'X-Test-Header', value: 'test_value' }
      ]
    };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/echoheader';
  targetUrl.search = 'X-Test-Header';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  const body =
      await executeAsyncScript(controlledframe, 'document.body.innerText');
  assert_equals(body, 'test_value');
}, 'WebRequest Inject Request Header');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  let responseHeaders = [];
  controlledframe.request.onCompleted.addListener(function(details) {
    responseHeaders = details.responseHeaders;
  }, {urls: [targetUrl.toString()]}, ['responseHeaders']);

  controlledframe.request.onHeadersReceived.addListener(function(details) {
    return {
      responseHeaders: [
        { name: 'X-Test-Header', value: 'test_value' }
      ]
    };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_equals(responseHeaders.length, 1);
  assert_equals(responseHeaders[0].name, 'X-Test-Header');
  assert_equals(responseHeaders[0].value, 'test_value');
}, 'WebRequest Inject Response Header');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onHeadersReceived.addListener(function(details) {
    const requestUrl = new URL(details.url);
    if (requestUrl.pathname === '/title1.html') {
      requestUrl.pathname = '/title2.html';
      return { redirectUrl: requestUrl.toString() };
    }
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyEvents([
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onBeforeRedirect',
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onResponseStarted',
    'onCompleted',
  ]);
  const path =
      await executeAsyncScript(controlledframe, 'location.pathname');
  assert_equals(path, '/title2.html');
}, 'WebRequest Inject Request Header');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  let requestHeaders = 'none';
  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  controlledframe.request.onSendHeaders.addListener(function(details) {
    requestHeaders = details.requestHeaders;
  }, {urls: [targetUrl.toString()]}, ['requestHeaders']);

  let requestHeadersExtra = 'none';
  controlledframe.request.onSendHeaders.addListener(function(details) {
    requestHeadersExtra = details.requestHeaders;
  }, {urls: [targetUrl.toString()]}, ['requestHeaders', 'extraHeaders']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_true(requestHeaders.length > 0);
  assert_true(requestHeaders.some((header) => header.name === 'User-Agent'));
  assert_false(
      requestHeaders.some((header) => header.name === 'Accept-Language'));

  assert_true(requestHeadersExtra.length > requestHeaders.length);
  assert_true(
      requestHeadersExtra.some((header) => header.name === 'Accept-Language'));
}, 'WebRequest Request Headers');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  let responseHeaders = 'none';
  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  controlledframe.request.onHeadersReceived.addListener(function(details) {
    responseHeaders = details.responseHeaders;
  }, {urls: [targetUrl.toString()]}, ['responseHeaders']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_true(responseHeaders.length >= 2);
  assert_true(responseHeaders.some((h) => h.name === 'Content-Length'));
  assert_true(responseHeaders.some((h) => h.name === 'Content-Type'));
}, 'WebRequest Response Headers');
