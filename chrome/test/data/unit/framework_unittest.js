// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class for testing the unit_test framework.
 * @constructor
 */
function FrameworkUnitTest() {}

FrameworkUnitTest.prototype = {
  __proto__: testing.Test.prototype,
};

TEST_F('FrameworkUnitTest', 'testAssertTrueOk', function() {
  assertTrue(true);
});

/**
 * Failing version of FrameworkUnitTest.
 * @constructor
 */
function FrameworkUnitTestFail() {}

FrameworkUnitTestFail.prototype = {
  __proto__: FrameworkUnitTest.prototype,

  /** inheritDoc */
  testShouldFail: true,
};

TEST_F('FrameworkUnitTestFail', 'testAssertFailFails', function() {
  assertNotReached();
});
