// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth1');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/auth-basic';
  targetUrl.search = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('authrequired', async (e) => {
    await new Promise((resolve) => {
      setTimeout(resolve, 1);
    });
    e.setCredentials({
      username: '',
      password: 'PASS',
    });
  });

  targetUrl.search = 'password=PASS&realm=REALM';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyWebRequestEvents([
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'authrequired',
    'responsestarted',
    'completed',
  ]);
}, 'WebRequest Auth');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth2');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/auth-basic';
  targetUrl.search = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  }).addEventListener('authrequired', (e) => {
    e.setCredentials({
      username: '',
      password: 'WRONG_PASSWORD',
    });
  });

  targetUrl.search = 'password=PASS&realm=REALM';
  try {
    await navigateControlledFrame(controlledframe, targetUrl.toString());
  } catch (e) {}

  assert_false(window.events.includes('completed'), 'completed fired');
  assert_true(window.events.includes('authrequired'), 'authrequired fired');
  assert_equals(
      window.events.slice(-1)[0], 'erroroccurred', 'erroroccurred fired');
  assert_equals(
      window.occurredErrors[0], 'net::ERR_TOO_MANY_RETRIES', 'error code');
}, 'WebRequest Auth Error');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth3');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/auth-basic';
  targetUrl.search = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  const interceptor = controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  });
  interceptor.addEventListener('authrequired', async (e) => {
    await new Promise((resolve) => {
      setTimeout(resolve, 1);
    });
    e.preventDefault();
  });

  let statusCode = -1;
  interceptor.addEventListener('completed', (e) => {
    statusCode = e.response.statusCode;
  });

  targetUrl.search = 'password=PASS&realm=REALM';
  await navigateControlledFrame(controlledframe, targetUrl.toString());

  verifyWebRequestEvents([
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'authrequired',
    'responsestarted',
    'completed',
  ]);
  assert_equals(statusCode, 401);
}, 'WebRequest Auth Cancel');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth4');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/auth-basic';
  targetUrl.search = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  const interceptor = controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    blocking: true,
  });
  interceptor.addEventListener('authrequired', (e) => {
    // Ignore the event
  });

  let statusCode = -1;
  interceptor.addEventListener('completed', (e) => {
    statusCode = e.response.statusCode;
  });

  targetUrl.search = 'password=PASS&realm=REALM';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/true);

  verifyWebRequestEvents([
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'authrequired',
    'responsestarted',
    'completed',
  ]);
  assert_equals(statusCode, 401);
}, 'WebRequest Auth Noop');
