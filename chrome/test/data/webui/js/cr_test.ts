// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebUiListener} from 'chrome://resources/js/cr.js';
import {addWebUiListener, removeWebUiListener, sendWithPromise, webUIListenerCallback, webUIResponse} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** Name of the chrome.send() message to be used in tests. */
const CHROME_SEND_NAME: string = 'echoMessage';

suite('CrSendWithPromiseTest', function() {
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

suite('CrWebUiListenersTest', function() {
  let listener1: WebUiListener|null = null;
  let listener2: WebUiListener|null = null;

  const EVENT_NAME: string = 'my-foo-event';

  teardown(function() {
    if (listener1) {
      removeWebUiListener(listener1);
    }
    if (listener2) {
      removeWebUiListener(listener2);
    }
  });

  test('removeWebUiListener', function() {
    listener1 = addWebUiListener(EVENT_NAME, function() {});
    assertTrue(removeWebUiListener(listener1));
    assertFalse(removeWebUiListener(listener1));
    assertFalse(removeWebUiListener({
      eventName: 'non-existing-event',
      uid: 12345,
    }));
  });

  test('addWebUiListener_ResponseParams', function() {
    const expectedString = 'foo';
    const expectedNumber = 123;
    const expectedArray = [1, 2];
    const expectedObject = {};

    return new Promise<void>(function(resolve) {
      listener1 = addWebUiListener(
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

  test('addWebUiListener_NoResponseParams', function() {
    return new Promise<void>(function(resolve) {
      listener1 = addWebUiListener(EVENT_NAME, function() {
        assertEquals(0, arguments.length);
        resolve();
      });
      webUIListenerCallback(EVENT_NAME);
    });
  });

  test('addWebUiListener_MulitpleListeners', function() {
    const resolver1 = new PromiseResolver();
    const resolver2 = new PromiseResolver();
    listener1 = addWebUiListener(EVENT_NAME, resolver1.resolve);
    listener2 = addWebUiListener(EVENT_NAME, resolver2.resolve);
    webUIListenerCallback(EVENT_NAME);
    // Check that both listeners registered are invoked.
    return Promise.all([resolver1.promise, resolver2.promise]);
  });
});
