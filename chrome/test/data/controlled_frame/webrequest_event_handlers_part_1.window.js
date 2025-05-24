// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/handbag.png';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  const script = `(async() => {
      const response = await fetch('${targetUrl.toString()}', {method:'GET'});
      await response.blob();
      })();`;
  await executeAsyncScript(controlledframe, script);

  verifyWebRequestEvents([
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
  addWebRequestListeners(controlledframe, targetUrl.toString());

  targetUrl.pathname = '/server-redirect';
  targetUrl.search = '/title1.html';
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
}, 'WebRequest Redirect');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html', 'auth1');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

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

  verifyWebRequestEvents([
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
  addWebRequestListeners(controlledframe, targetUrl.toString());

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

  verifyWebRequestEvents([
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
  addWebRequestListeners(controlledframe, targetUrl.toString());

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
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onBeforeRequest.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyWebRequestEvents(['onBeforeRequest', 'onErrorOccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeRequest');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  addWebRequestListeners(controlledframe, targetUrl.toString());

  controlledframe.request.onBeforeSendHeaders.addListener(function(details) {
    return { cancel: true };
  }, { urls: [targetUrl.toString()] }, ['blocking']);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString(), /*expectFailure=*/ true);

  verifyWebRequestEvents(['onBeforeRequest', 'onBeforeSendHeaders', 'onErrorOccurred']);
  assert_equals(window.occurredErrors[0], 'net::ERR_BLOCKED_BY_CLIENT');
}, 'WebRequest Cancel beforeSendHeaders');
