// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

function WebUIAssertionsTest() {}

WebUIAssertionsTest.prototype = {
  __proto__: testing.Test.prototype,
  browsePreload: DUMMY_URL,
};

TEST_F('WebUIAssertionsTest', 'testConstructedMessage', function() {
  const message = 'myErrorMessage';
  const result = runTestFunction('testConstructMessage', function() {
    assertTrue(false, message);
  }, []);
  resetTestState();
  assertNotEquals(
      -1, result[1].indexOf(message + ': expected false to be true'));
});

/**
 * Failing version of WebUIAssertionsTest.
 * @extends WebUIAssertionsTest
 * @constructor
 */
function WebUIAssertionsTestFail() {}

WebUIAssertionsTestFail.prototype = {
  __proto__: WebUIAssertionsTest.prototype,

  /** @inheritDoc */
  testShouldFail: true,
};

// Test that an assertion failure fails test.
TEST_F('WebUIAssertionsTestFail', 'testAssertFailFails', function() {
  assertNotReached();
});

/**
 * Async version of WebUIAssertionsTestFail.
 * @extends WebUIAssertionsTest
 * @constructor
 */
function WebUIAssertionsTestAsyncFail() {}

WebUIAssertionsTestAsyncFail.prototype = {
  __proto__: WebUIAssertionsTestFail.prototype,

  /** @inheritDoc */
  isAsync: true,
};

// Test that an assertion failure doesn't hang forever.
TEST_F('WebUIAssertionsTestAsyncFail', 'testAsyncFailCallsDone', function() {
  assertNotReached();
});
