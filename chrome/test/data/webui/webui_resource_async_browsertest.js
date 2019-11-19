// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Framework for running async JS tests for cr.js utility methods.
 */

/** @const {string} Name of the chrome.send() message to be used in tests. */
var CHROME_SEND_NAME = 'echoMessage';

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function WebUIResourceAsyncTest() {}

WebUIResourceAsyncTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: DUMMY_URL,

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//ui/webui/resources/js/cr.js',
  ],
};

TEST_F('WebUIResourceAsyncTest', 'SendWithPromise', function() {
  /**
   * TODO(dpapad): Move this helper method in test_api.js.
   * @param {string} name chrome.send message name.
   * @return {!Promise} Fires when chrome.send is called with the given message
   *     name.
   */
  function whenChromeSendCalled(name) {
    return new Promise(function(resolve, reject) {
      registerMessageCallback(name, null, resolve);
    });
  }

  suite('SendWithPromise', function() {
    var rejectPromises = false;

    setup(function() {
      // Simulate a WebUI handler that echoes back all parameters passed to it.
      // Rejects the promise depending on |rejectPromises|.
      whenChromeSendCalled(CHROME_SEND_NAME).then(function(args) {
        var callbackId = args[0];
        cr.webUIResponse.apply(
            null, [callbackId, !rejectPromises].concat(args.slice(1)));
      });
    });
    teardown(function() {
      rejectPromises = false;
    });

    test('sendWithPromise_ResponseObject', function() {
      var expectedResponse = {'foo': 'bar'};
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse)
          .then(function(response) {
            assertEquals(
                JSON.stringify(expectedResponse), JSON.stringify(response));
          });
    });

    test('sendWithPromise_ResponseArray', function() {
      var expectedResponse = ['foo', 'bar'];
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse)
          .then(function(response) {
            assertEquals(
                JSON.stringify(expectedResponse), JSON.stringify(response));
          });
    });

    test('sendWithPromise_ResponsePrimitive', function() {
      var expectedResponse = 1234;
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse)
          .then(function(response) {
            assertEquals(expectedResponse, response);
          });
    });

    test('sendWithPromise_ResponseVoid', function() {
      return cr.sendWithPromise(CHROME_SEND_NAME).then(function(response) {
        assertEquals(undefined, response);
      });
    });

    test('sendWithPromise_Reject', function() {
      rejectPromises = true;
      var expectedResponse = 1234;
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse)
          .then(
              function() {
                assertNotReached('should have rejected promise');
              },
              function(error) {
                assertEquals(expectedResponse, error);
              });
    });
  });

  // Run all registered tests.
  mocha.run();
});


TEST_F('WebUIResourceAsyncTest', 'WebUIListeners', function() {
  suite('WebUIListeners', function() {
    var listener1 = null;
    var listener2 = null;

    /** @const {string} */
    var EVENT_NAME = 'my-foo-event';

    teardown(function() {
      if (listener1) {
        cr.removeWebUIListener(listener1);
      }
      if (listener2) {
        cr.removeWebUIListener(listener2);
      }
    });

    test('removeWebUIListener', function() {
      listener1 = cr.addWebUIListener(EVENT_NAME, function() {});
      assertTrue(cr.removeWebUIListener(listener1));
      assertFalse(cr.removeWebUIListener(listener1));
      assertFalse(cr.removeWebUIListener({
        eventName: 'non-existing-event',
        uid: 12345,
      }));
    });

    test('addWebUIListener_ResponseParams', function() {
      var expectedString = 'foo';
      var expectedNumber = 123;
      var expectedArray = [1, 2];
      var expectedObject = {};

      return new Promise(function(resolve, reject) {
        listener1 = cr.addWebUIListener(EVENT_NAME, function(s, n, a, o) {
          assertEquals(expectedString, s);
          assertEquals(expectedNumber, n);
          assertEquals(expectedArray, a);
          assertEquals(expectedObject, o);
          resolve();
        });
        cr.webUIListenerCallback(
            EVENT_NAME, expectedString, expectedNumber, expectedArray,
            expectedObject);
      });
    });

    test('addWebUIListener_NoResponseParams', function() {
      return new Promise(function(resolve, reject) {
        listener1 = cr.addWebUIListener(EVENT_NAME, function() {
          assertEquals(0, arguments.length);
          resolve();
        });
        cr.webUIListenerCallback(EVENT_NAME);
      });
    });

    test('addWebUIListener_MulitpleListeners', function() {
      var resolver1 = new PromiseResolver();
      var resolver2 = new PromiseResolver();
      listener1 = cr.addWebUIListener(EVENT_NAME, resolver1.resolve);
      listener2 = cr.addWebUIListener(EVENT_NAME, resolver2.resolve);
      cr.webUIListenerCallback(EVENT_NAME);
      // Check that both listeners registered are invoked.
      return Promise.all([resolver1.promise, resolver2.promise]);
    });
  });

  // Run all registered tests.
  mocha.run();
});
