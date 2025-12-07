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

  let request = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
  }).addEventListener('beforerequest', (e) => {
    request = e.request;
  });

  await executeAsyncScript(controlledframe, `
    fetch('${targetUrl.toString()}', {
      method: 'POST',
      body: 'request body',
    }).then(() => {
      // The Response object can't be serialized and sent back to the embedder,
      // so drop it and convert to a Promise<undefined>.
    });
  `);

  assert_true(!!request, 'beforerequest event not received');
  assert_equals(request.body, undefined, 'request.body');
}, 'WebRequest No Body');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';

  let request = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    includeRequestBody: true,
  }).addEventListener('beforerequest', (e) => {
    request = e.request;
  });

  await executeAsyncScript(controlledframe, `
    fetch('${targetUrl.toString()}', {
      method: 'POST',
      body: 'request body',
    }).then(() => {
      // The Response object can't be serialized and sent back to the embedder,
      // so drop it and convert to a Promise<undefined>.
    });
  `);

  assert_true(!!request, 'beforerequest event not received');
  assert_equals(request.method, 'POST', 'request.method');
  assert_equals(request.type, 'xmlhttprequest', 'request.type');
  assert_equals(request.url, targetUrl.toString(), 'request.url');
  assert_equals(
      JSON.stringify(request.body), '{"raw":[{"bytes":{}}]}', 'request.body');
  const bodyString = new TextDecoder().decode(request.body.raw[0].bytes);
  assert_equals(bodyString, 'request body', 'request.body contents');
}, 'WebRequest Body String');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';

  let request = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    includeRequestBody: true,
  }).addEventListener('beforerequest', (e) => {
    request = e.request;
  });

  await executeAsyncScript(controlledframe, `
    const formData = new FormData();
    formData.append('key1', 'value1');
    formData.append('key1', 'value11');
    formData.append('key2', 'value2');
    fetch('${targetUrl.toString()}', {
      method: 'POST',
      body: formData,
    }).then(() => {
      // The Response object can't be serialized and sent back to the embedder,
      // so drop it and convert to a Promise<undefined>.
    });
  `);

  assert_true(!!request, 'beforerequest event not received');
  assert_equals(request.method, 'POST', 'request.method');
  assert_equals(request.type, 'xmlhttprequest', 'request.type');
  assert_equals(request.url, targetUrl.toString(), 'request.url');
  assert_equals(
      JSON.stringify(request.body),
      JSON.stringify(
          {formData: {key1: ['value1', 'value11'], key2: ['value2']}}),
      'request.body');
}, 'WebRequest Body FormData');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';

  let requestHeaders = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    includeHeaders: 'cors',
  }).addEventListener('sendheaders', (e) => {
    requestHeaders = e.request.headers;
  });

  let requestHeadersExtra = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    includeHeaders: 'all',
  }).addEventListener('sendheaders', (e) => {
    requestHeadersExtra = e.request.headers;
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_true(requestHeaders.has('User-Agent'));
  assert_false(requestHeaders.has('Accept-Language'));

  assert_true(requestHeadersExtra.has('Accept-Language'));
}, 'WebRequest Get Request Headers');

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/title1.html';

  let responseHeaders = null;
  controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl.toString()],
    includeHeaders: 'cors',
  }).addEventListener('headersreceived', (e) => {
    responseHeaders = e.response.headers;
  });

  await navigateControlledFrame(controlledframe, targetUrl.toString());

  assert_true(responseHeaders.has('Content-Length'));
  assert_true(responseHeaders.has('Content-Type'));
}, 'WebRequest Get Response Headers');
