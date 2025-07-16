// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('beforerequest', (e) => {
    e.preventDefault();
  });

  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/true);

  verifyWebRequestEvents(['beforerequest', 'erroroccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeRequest');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('beforesendheaders', (e) => {
    e.preventDefault();
  });

  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/true);

  verifyWebRequestEvents(
      ['beforerequest', 'beforesendheaders', 'erroroccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeSendHeaders');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('headersreceived', (e) => {
    e.preventDefault();
  });

  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/true);

  verifyWebRequestEvents([
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'erroroccurred'
  ]);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel headersReceived');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/echoheader';
  targetUrl.search = '*';

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('beforesendheaders', (e) => {
    e.setRequestHeaders({'X-Test-Header': 'test_value'});
  });

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
  targetUrl.pathname = '/title1.html';

  const interceptor = controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
    includeHeaders: 'cors',
  });
  interceptor.addEventListener('headersreceived', (e) => {
    const customHeaders = new Headers();
    customHeaders.append('X-Test-Header', 'test_value1');
    customHeaders.append('X-Test-Header', 'test_value2\x01\x02\x03');
    e.setResponseHeaders(customHeaders);
  });

  let responseHeaders = [];
  interceptor.addEventListener('completed', (e) => {
    responseHeaders = e.response.headers;
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  const headerEntries = [...responseHeaders.entries()];
  assert_equals(headerEntries.length, 1);
  assert_equals(headerEntries[0][0], 'x-test-header');
  assert_equals(headerEntries[0][1], 'test_value1, test_value2\x01\x02\x03');
}, 'WebRequest Inject Response Header');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title2.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());
  targetUrl.pathname = '/title1.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('beforerequest', (e) => {
    const requestUrl = new URL(e.request.url);
    if (requestUrl.pathname === '/title1.html') {
      requestUrl.pathname = '/title2.html';
      e.redirect(requestUrl.toString());
    }
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyWebRequestEvents([
    'beforerequest',
    'beforeredirect',
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'responsestarted',
    'completed',
  ]);
  const path =
      await executeAsyncScript(controlledframe, 'location.pathname');
  assert_equals(path, '/title2.html');
}, 'WebRequest Redirect in BeforeRequestEvent');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title2.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());
  targetUrl.pathname = '/title1.html';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('headersreceived', (e) => {
    const requestUrl = new URL(e.request.url);
    if (requestUrl.pathname === '/title1.html') {
      requestUrl.pathname = '/title2.html';
      e.redirect(requestUrl.toString());
    }
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyWebRequestEvents([
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'beforeredirect',
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'responsestarted',
    'completed',
  ]);
  const path =
      await executeAsyncScript(controlledframe, 'location.pathname');
  assert_equals(path, '/title2.html');
}, 'WebRequest Redirect in HeadersReceivedEvent');
