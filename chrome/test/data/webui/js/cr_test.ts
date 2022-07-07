// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener, webUIListenerCallback, webUIResponse} from 'chrome://resources/js/cr.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** Name of the chrome.send() message to be used in tests. */
const CHROME_SEND_NAME: string = 'echoMessage';

suite('CrModuleSendWithPromiseTest', function() {
  const originalChromeSend = chrome.send;
  let rejectPromises = false;

  function whenChromeSendCalled(_name: string): Promise<any[]> {
    assertEquals(originalChromeSend, chrome.send);
    return new Promise(function(resolve) {
      chrome.send = (_msg: string, args?: any[]) => {
        resolve(args!);
      };
    });
  }

  setup(function() {
    // Simulate a WebUI handler that echoes back all parameters passed to it.
    // Rejects the promise depending on |rejectPromises|.
    whenChromeSendCalled(CHROME_SEND_NAME).then(function(args: any[]) {
      const callbackId = args[0];
      const response = args[1];
      webUIResponse(callbackId, !rejectPromises, response);
    });
  });

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
    class Foo {}
    addSingletonGetter(Foo);

    type FooWithGetInstance =
        typeof Foo&{getInstance: () => Foo, instance_?: Foo | null};

    assertEquals(
        'function', typeof (Foo as FooWithGetInstance).getInstance,
        'Should add get instance function');

    const x = (Foo as FooWithGetInstance).getInstance();
    assertEquals('object', typeof x, 'Should successfully create an object');
    assertNotEquals(null, x, 'Created object should not be null');

    const y = (Foo as FooWithGetInstance).getInstance();
    assertEquals(x, y, 'Should return the same object');

    delete (Foo as FooWithGetInstance).instance_;

    const z = (Foo as FooWithGetInstance).getInstance();
    assertEquals('object', typeof z, 'Should work after clearing for testing');
    assertNotEquals(null, z, 'Created object should not be null');

    assertNotEquals(
        x, z, 'Should return a different object after clearing for testing');
  });
});

suite('CrModuleWebUIListenersTest', function() {
  let listener1: WebUIListener|null = null;
  let listener2: WebUIListener|null = null;

  const EVENT_NAME: string = 'my-foo-event';

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

    return new Promise<void>(function(resolve) {
      listener1 = addWebUIListener(
          EVENT_NAME, function(s: string, n: number, a: number[], o: object) {
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
    return new Promise<void>(function(resolve) {
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
