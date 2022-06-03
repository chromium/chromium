// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, addWebUIListener, removeWebUIListener, sendWithPromise, webUIListenerCallback, webUIResponse} from 'chrome://resources/js/cr.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/** @type {string} Name of the chrome.send() message to be used in tests. */
const CHROME_SEND_NAME = 'echoMessage';

suite('CrModuleSendWithPromiseTest', function() {
  const originalChromeSend = chrome.send;
  let rejectPromises = false;

  function whenChromeSendCalled(name) {
    assertEquals(originalChromeSend, chrome.send);
    return new Promise(function(resolve, reject) {
      chrome.send = (_, args) => resolve(args);
    });
  }

  /** @override */
  setup(function() {
    // Simulate a WebUI handler that echoes back all parameters passed to it.
    // Rejects the promise depending on |rejectPromises|.
    whenChromeSendCalled(CHROME_SEND_NAME).then(function(args) {
      const callbackId = args[0];
      webUIResponse.apply(
          null, [callbackId, !rejectPromises].concat(args.slice(1)));
    });
  });

  /** @override */
  teardown(function() {
    rejectPromises = false;

    // Restore original chrome.send(), as it is necessary for the testing
    // framework to signal test completion.
    chrome.send = originalChromeSend;
  });

  test('ResponseObject', function() {
    const expectedResponse = {'foo': 'bar'};
    return sendWithPromise(CHROME_SEND_NAME, expectedResponse)
        .then(function(response) {
          assertEquals(
              JSON.stringify(expectedResponse), JSON.stringify(response));
        });
  });

  test('ResponseArray', function() {
    const expectedResponse = ['foo', 'bar'];
    return sendWithPromise(CHROME_SEND_NAME, expectedResponse)
        .then(function(response) {
          assertEquals(
              JSON.stringify(expectedResponse), JSON.stringify(response));
        });
  });

  test('ResponsePrimitive', function() {
    const expectedResponse = 1234;
    return sendWithPromise(CHROME_SEND_NAME, expectedResponse)
        .then(function(response) {
          assertEquals(expectedResponse, response);
        });
  });

  test('ResponseVoid', function() {
    return sendWithPromise(CHROME_SEND_NAME).then(function(response) {
      assertEquals(undefined, response);
    });
  });

  test('Reject', function() {
    rejectPromises = true;
    const expectedResponse = 1234;
    return sendWithPromise(CHROME_SEND_NAME, expectedResponse)
        .then(
            function() {
              assertNotReached('should have rejected promise');
            },
            function(error) {
              assertEquals(expectedResponse, error);
            });
  });
});

suite('CrModuleAddSingletonGetterTest', function() {
  test('addSingletonGetter', function() {
    function Foo() {}
    addSingletonGetter(Foo);

    assertEquals(
        'function', typeof Foo.getInstance, 'Should add get instance function');

    const x = Foo.getInstance();
    assertEquals('object', typeof x, 'Should successfully create an object');
    assertNotEquals(null, x, 'Created object should not be null');

    const y = Foo.getInstance();
    assertEquals(x, y, 'Should return the same object');

    delete Foo.instance_;

    const z = Foo.getInstance();
    assertEquals('object', typeof z, 'Should work after clearing for testing');
    assertNotEquals(null, z, 'Created object should not be null');

    assertNotEquals(
        x, z, 'Should return a different object after clearing for testing');
  });
});

suite('CrModuleWebUIListenersTest', function() {
  let listener1 = null;
  let listener2 = null;

  /** @const {string} */
  const EVENT_NAME = 'my-foo-event';

  teardown(function() {
    if (listener1) {
      removeWebUIListener(listener1);
    }
    if (listener2) {
      removeWebUIListener(listener2);
    }
  });

  test('removeWebUIListener', function() {
    listener1 = addWebUIListener(EVENT_NAME, function() {});
    assertTrue(removeWebUIListener(listener1));
    assertFalse(removeWebUIListener(listener1));
    assertFalse(removeWebUIListener({
      eventName: 'non-existing-event',
      uid: 12345,
    }));
  });

  test('addWebUIListener_ResponseParams', function() {
    const expectedString = 'foo';
    const expectedNumber = 123;
    const expectedArray = [1, 2];
    const expectedObject = {};

    return new Promise(function(resolve, reject) {
      listener1 = addWebUIListener(EVENT_NAME, function(s, n, a, o) {
        assertEquals(expectedString, s);
        assertEquals(expectedNumber, n);
        assertEquals(expectedArray, a);
        assertEquals(expectedObject, o);
        resolve();
      });
      webUIListenerCallback(
          EVENT_NAME, expectedString, expectedNumber, expectedArray,
          expectedObject);
    });
  });

  test('addWebUIListener_NoResponseParams', function() {
    return new Promise(function(resolve, reject) {
      listener1 = addWebUIListener(EVENT_NAME, function() {
        assertEquals(0, arguments.length);
        resolve();
      });
      webUIListenerCallback(EVENT_NAME);
    });
  });

  test('addWebUIListener_MulitpleListeners', function() {
    const resolver1 = new PromiseResolver();
    const resolver2 = new PromiseResolver();
    listener1 = addWebUIListener(EVENT_NAME, resolver1.resolve);
    listener2 = addWebUIListener(EVENT_NAME, resolver2.resolve);
    webUIListenerCallback(EVENT_NAME);
    // Check that both listeners registered are invoked.
    return Promise.all([resolver1.promise, resolver2.promise]);
  });
});
