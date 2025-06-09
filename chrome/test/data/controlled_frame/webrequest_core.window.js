// Copyright 2025 The Chromium Authors
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
    'beforerequest',
    'beforesendheaders',
    'sendheaders',
    'headersreceived',
    'responsestarted',
    'completed',
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
}, 'WebRequest Redirect');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';

  let completedEvent = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
  }).addEventListener('completed', (e) => {
    completedEvent = e;
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_true(!!completedEvent, 'completed event not received');
  assert_equals(
      completedEvent.documentLifecycle, 'active', 'documentLifecycle');
  assert_equals(completedEvent.frameId, 0, 'frameId');
  assert_equals(completedEvent.frameType, 'outermost-frame', 'frameType');
  assert_equals(completedEvent.parentFrameId, -1, 'parentFrameId');
  assert_equals(completedEvent.request.method, 'GET', 'request.method');
  assert_equals(completedEvent.request.type, 'main-frame', 'request.type');
  assert_equals(
      completedEvent.request.url, targetUrl.toString(), 'request.url');
  assert_true(completedEvent.request.id > 0, 'request.id');
  assert_true(!!completedEvent.response.ip, 'response.ip');
  assert_equals(
      typeof completedEvent.response.fromCache, 'boolean',
      'response.fromCache type');
  assert_equals(completedEvent.response.statusCode, 200, 'response.statusCode');
  assert_equals(
      completedEvent.response.statusLine, 'HTTP/1.1 200 OK',
      'response.statusLine');
}, 'WebRequest Details');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '*';
  const interceptor = controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    resourceTypes: ['main-frame'],
  });

  const handler1RequestUrls = [];
  const handler1 = (e) => {
    handler1RequestUrls.push(e.request.url);
  };
  interceptor.addEventListener('beforerequest', handler1);

  const handler2RequestUrls = [];
  const handler2 = (e) => {
    handler2RequestUrls.push(e.request.url);
  };
  interceptor.addEventListener('beforerequest', handler2);

  targetUrl.pathname = '/title1.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString());
  assert_equals(handler1RequestUrls.length, 1, 'handler1RequestUrls length');
  assert_equals(
      handler1RequestUrls[0], targetUrl.toString(),
      'handler1RequestUrls values');
  assert_equals(handler2RequestUrls.length, 1, 'handler2RequestUrls length');
  assert_equals(
      handler2RequestUrls[0], targetUrl.toString(),
      'handler2RequestUrls values');

  interceptor.removeEventListener('beforerequest', handler2);

  targetUrl.pathname = '/title2.html';
  await navigateControlledFrame(
      controlledframe, targetUrl.toString());
  assert_equals(handler1RequestUrls.length, 2, 'handler1RequestUrls length');
  assert_equals(
      handler1RequestUrls[1], targetUrl.toString(),
      'handler1RequestUrls values');
  assert_equals(handler2RequestUrls.length, 1, 'handler2RequestUrls length');
}, 'WebRequest Unregister Listener');
