// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generates a unique authentication URL so each test can run
// without hitting the HTTP authentication cache. Each test
// must use a unique realm, however.
function getURLAuthRequired(realm, subpath = 'subpath') {
  return getServerURL(
      'auth-basic/' + realm + '/' + subpath + '?realm=' + realm);
}

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Test that two parallel onAuthRequired signals do not interfere with each
  // other. This is a regression test for https://crbug.com/931479.
  function authRequiredParallel() {
    const realm = 'parallelasync';

    const imgUrl1 = getURLAuthRequired(realm, '1');
    const imgUrl2 = getURLAuthRequired(realm, '2');
    const initiator = getServerDomain(initiators.WEB_INITIATED);
    const parallelAuthRequestsUrl = getServerURL(
        'extensions/api_test/webrequest/auth_parallel?img1=' +
        encodeURIComponent(imgUrl1) + '&img2=' + encodeURIComponent(imgUrl2));

    function createExternallyResolvablePromise() {
      let _resolve;
      const promise = new Promise((resolve, reject) => _resolve = resolve);
      promise.resolve = _resolve;
      return promise;
    }

    // Create some promises that will resolved to callbacks when onAuthRequired
    // and onComplete fire.
    const authRequired1 = createExternallyResolvablePromise();
    const completed1 = createExternallyResolvablePromise();
    const authRequired2 = createExternallyResolvablePromise();

    // responseStarted2 is whether the request for |imgUrl2| has signalled
    // |onResponseStarted| yet.
    let responseStarted2 = false;

    expect(
      [  // events
        { label: 'onBeforeRequest-1',
          event: 'onBeforeRequest',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            // The testing framework cannot recover the frame URL because the
            // URL filter below does not capture the top-level request.
            frameUrl: 'unknown frame URL',
          },
        },
        { label: 'onBeforeSendHeaders-1',
          event: 'onBeforeSendHeaders',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            // Note: no requestHeaders because we don't ask for them.
          },
          retval: {}
        },
        { label: 'onSendHeaders-1',
          event: 'onSendHeaders',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
          }
        },
        { label: 'onHeadersReceived-1',
          event: 'onHeadersReceived',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
            statusCode: 401,
          }
        },
        { label: 'onAuthRequired-1',
          event: 'onAuthRequired',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            isProxy: false,
            scheme: 'basic',
            realm: realm,
            challenger: {host: testServer, port: testServerPort},
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
            statusCode: 401,
          },
          retval_function:
              (name, details, callback) => authRequired1.resolve(callback),
        },
        { label: 'onResponseStarted-1',
          event: 'onResponseStarted',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            fromCache: false,
            statusCode: 200,
            ip: '127.0.0.1',
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
          }
        },
        { label: 'onCompleted-1',
          event: 'onCompleted',
          details: {
            url: imgUrl1,
            type: 'image',
            initiator: initiator,
            fromCache: false,
            statusCode: 200,
            ip: '127.0.0.1',
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 200 OK',
          },
          retval_function: (name, details) => completed1.resolve(),
        },
        { label: 'onBeforeRequest-2',
          event: 'onBeforeRequest',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            // The testing framework cannot recover the frame URL because the
            // URL filter below does not capture the top-level request.
            frameUrl: 'unknown frame URL',
          },
        },
        { label: 'onBeforeSendHeaders-2',
          event: 'onBeforeSendHeaders',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            // Note: no requestHeaders because we don't ask for them.
          },
          retval: {}
        },
        { label: 'onSendHeaders-2',
          event: 'onSendHeaders',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
          }
        },
        { label: 'onHeadersReceived-2',
          event: 'onHeadersReceived',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
            statusCode: 401,
          }
        },
        { label: 'onAuthRequired-2',
          event: 'onAuthRequired',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            isProxy: false,
            scheme: 'basic',
            realm: realm,
            challenger: {host: testServer, port: testServerPort},
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
            statusCode: 401,
          },
          retval_function:
              (name, details, callback) => authRequired2.resolve(callback),
        },
        { label: 'onResponseStarted-2',
          event: 'onResponseStarted',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            fromCache: false,
            statusCode: 401,
            ip: '127.0.0.1',
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
          },
          retval_function: (name, details) => responseStarted2 = true,
        },
        { label: 'onCompleted-2',
          event: 'onCompleted',
          details: {
            url: imgUrl2,
            type: 'image',
            initiator: initiator,
            fromCache: false,
            statusCode: 401,
            ip: '127.0.0.1',
            responseHeadersExist: true,
            statusLine: 'HTTP/1.1 401 Unauthorized',
          }
        },
      ],
      [  // event order
        ['onBeforeRequest-1', 'onBeforeSendHeaders-1', 'onSendHeaders-1',
         'onHeadersReceived-1', 'onAuthRequired-1', 'onResponseStarted-1',
         'onCompleted-1'],
        ['onBeforeRequest-2', 'onBeforeSendHeaders-2', 'onSendHeaders-2',
         'onHeadersReceived-2', 'onAuthRequired-2', 'onResponseStarted-2',
         'onCompleted-2']
      ],
      // Only pay attention to |imgUrl1| and |imgUrl2|, not
      // |parallelAuthRequestsUrl|.
      {urls: ['<all_urls>'], types: ['image']},
      ['responseHeaders', 'asyncBlocking']);

    navigateAndWait(parallelAuthRequestsUrl);
    (async function() {
      // Wait for onAuthRequired to be signaled for both requests before doing
      // anything.
      const callback1 = await authRequired1;
      const callback2 = await authRequired2;

      // Resolve the first request and let it complete.
      callback1({authCredentials: {username: 'foo', password: 'secret'}});
      await completed1;

      // We wish to test that this did not resolve |callback2| with the same
      // credentials internally. This would cause |imgUrl2| to signal
      // |onResponseStarted| and |onCompleted| though it should be blocked.
      chrome.test.assertFalse(responseStarted2);

      // However it's possible that the second request may be much slower than
      // the first, causing the above check to pass even if we don't correctly
      // isolate the two authentication requests.
      //
      // Per the webRequest documentation, |onBeforeSendHeaders| should be
      // signaled after a resolved |onAuthRequired|. This happens without a
      // network delay, so it would make a fairly reliable test. However,
      // webRequest does not match the documentation and does not signal it. See
      // https://crbug.com/809761.
      //
      // Instead, resolve the second request, this time canceling the
      // authentication request. This should result in |imgUrl2| completing with
      // a 401 response. If the credentials were incorrectly propagated from
      // |imgUrl1|, this will be ignored and |imgUrl2| will complete with a 200
      // response.
      callback2({cancel: true});
    })();
  },
])});
