// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onHeadersReceived.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyWebRequestEvents([
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
  addWebRequestListeners(controlledframe, targetUrl.toString());

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
  addWebRequestListeners(controlledframe, targetUrl.toString());

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
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onHeadersReceived.addListener(function(details) {
    const requestUrl = new URL(details.url);
    if (requestUrl.pathname === '/title1.html') {
      requestUrl.pathname = '/title2.html';
      return { redirectUrl: requestUrl.toString() };
    }
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyWebRequestEvents([
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
