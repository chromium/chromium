// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

// This file contains authrequired event related tests.
//
// Successful authentications are cached and can mess with subsequent tests.
// To get around this, the test uses separate partitions for tests that
// successfully authenticate, but a shared partition for others to speed up the
// tests because partition creation is expensive.

async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

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
  }).addEventListener('authrequired', (e) => {
    e.setCredentials(new Promise((resolve) => {
      // Sleep to ensure the network request is paused until the
      // promise resolves.
      sleep(1);
      resolve({
        username: '',
        password: 'PASS',
      });
    }));
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
}, 'WebRequest Auth Async');

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
}, 'WebRequest Auth Sync');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'noauth');
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
}, 'WebRequest Auth Fail');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'noauth');
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
}, 'WebRequest Auth Cancel Sync');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'noauth');
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
    const controller = new AbortController();
    e.setCredentials(new Promise(() => {}), {signal: controller.signal});
    sleep(1).then(() => {
      controller.abort();
    });
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
}, 'WebRequest Auth Cancel AbortSignal');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'noauth');
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
    const controller = new AbortController();
    controller.abort();
    e.setCredentials(new Promise(() => {}), {signal: controller.signal});
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
}, 'WebRequest Auth Cancel Preaborted AbortSignal');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'noauth');
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
