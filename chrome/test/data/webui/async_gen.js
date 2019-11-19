// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for generated async tests.
 * @extends {testing.Test}
 */
function WebUIBrowserAsyncGenTest() {}

WebUIBrowserAsyncGenTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Define the C++ class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: null,

  /** @inheritDoc */
  tearDown: function() {
    expectFalse(this.tornDown);
    expectFalse(this.running);
    this.tornDown = true;
    chrome.send('tearDown');
    testing.Test.prototype.tearDown.call(this);
  },

  /** @inheritDoc */
  browsePreload: DUMMY_URL,

  /** @inheritDoc */
  isAsync: true,

  /**
   * True when the tearDown method is called.
   * @type {boolean}
   */
  tornDown: false,

  /**
   * True when running sync portion of test.
   * @type {boolean}
   */
  running: false,

  /** @inheritDoc */
  preLoad: function() {
    if (window.preLoadCount === undefined) {
      window.preLoadCount = 0;
    }
    assertEquals(0, Number(window.preLoadCount++));
  },
};

// Include the c++ test fixture.
GEN('#include "chrome/test/data/webui/async_gen.h"');

/**
 * Will be set to continuation test #1.
 * @type {Function}
 * @this {WebUIBrowserAsyncGenTest}
 */
var continueTest;

/**
 * Will be set to continuation test #2.
 * @type {Function}
 * @this {WebUIBrowserAsyncGenTest}
 */
var continueTest2;

TEST_F('WebUIBrowserAsyncGenTest', 'TestPreloadOnceOnNavigate', function() {
  window.addEventListener(
      'hashchange', this.continueTest(WhenTestDone.DEFAULT, function() {
        testDone();
      }));
  window.location = DUMMY_URL + '#anchor';
});

// Test that tearDown isn't called until the callback test runs.
TEST_F('WebUIBrowserAsyncGenTest', 'TestTearDown', function() {
  assertFalse(this.tornDown);
  this.running = true;
  continueTest = this.continueTest(WhenTestDone.ALWAYS, function() {
    this.running = false;
  });
  chrome.send('callJS', ['continueTest']);
});

// Test that continuing can be done multiple times and have access to closure
// variables.
TEST_F('WebUIBrowserAsyncGenTest', 'TestContinue', function() {
  var xyz = false;
  continueTest = this.continueTest(WhenTestDone.DEFAULT, function() {
    assertFalse(xyz);
    xyz = true;
    chrome.send('callJS', ['continueTest2']);
  });
  continueTest2 = this.continueTest(WhenTestDone.ALWAYS, function() {
    assertTrue(xyz);
  });
  chrome.send('callJS', ['continueTest']);
});

// Test that runAllActionsAsync can be called with multiple functions, and with
// bound, saved, or mixed arguments.
TEST_F('WebUIBrowserAsyncGenTest', 'TestRunAllActionsAsyncMock', function() {
  this.makeAndRegisterMockHandler([
    'testBoundArgs',
    'testSavedArgs',
    'testMixedArgs',
  ]);
  // Bind some arguments.
  var var1, var2;
  this.mockHandler.expects(once()).testBoundArgs().will(
      runAllActionsAsync(WhenTestDone.DEFAULT, callFunction(function(args) {
                           var1 = args[0];
                         }, ['val1']), callFunction(function(args) {
                           var2 = args[0];
                         }, ['val2'])));

  // Receive some saved arguments.
  var var3, var4;
  var savedArgs = new SaveMockArguments();
  var savedArgs2 = new SaveMockArguments();
  this.mockHandler.expects(once())
      .testSavedArgs(savedArgs.match(savedArgs2.match(eq(['passedVal1']))))
      .will(runAllActionsAsync(
          WhenTestDone.DEFAULT,
          callFunctionWithSavedArgs(savedArgs, function(args) {
            var3 = args[0];
          }), callFunctionWithSavedArgs(savedArgs2, function(args) {
            var4 = args[0];
          })));

  // Receive some saved arguments and some bound arguments.
  var var5, var6, var7, var8;
  this.mockHandler.expects(once())
      .testMixedArgs(savedArgs.match(savedArgs2.match(eq('passedVal2'))))
      .will(runAllActionsAsync(
          WhenTestDone.DEFAULT,
          callFunctionWithSavedArgs(
              savedArgs,
              function(passedArgs, boundArgs) {
                var5 = passedArgs[0];
                var6 = boundArgs[0];
              },
              ['val6']),
          callFunctionWithSavedArgs(
              savedArgs2, function(passedArgs, boundArgs) {
                var7 = passedArgs[0];
                var8 = boundArgs[0];
              }, ['val8'])));

  // Send the cases to the mocked handler & tell the C++ handler to continue2.
  continueTest = this.continueTest(WhenTestDone.ASSERT, function() {
    chrome.send('testBoundArgs');
    chrome.send('testSavedArgs', ['passedVal1']);
    chrome.send('testMixedArgs', ['passedVal2']);
    chrome.send('callJS', ['continueTest2']);
  });

  // Check expectations after mocks have been called.
  continueTest2 = this.continueTest(WhenTestDone.ALWAYS, function() {
    expectEquals('val1', var1);
    expectEquals('val2', var2);
    expectEquals('passedVal1', var3);
    expectEquals('passedVal1', var4);
    expectEquals('passedVal2', var5);
    expectEquals('val6', var6);
    expectEquals('passedVal2', var7);
    expectEquals('val8', var8);
  });

  // Kick off the tests asynchronously.
  chrome.send('callJS', ['continueTest']);
});

/**
 * Set to true when |setTestRanTrue| is called.
 */
var testRan = false;

/**
 * Set |testRan| to true.
 */
function setTestRanTrue() {
  testRan = true;
}

/**
 * Will be set to the runTest continuation by the following test fixture.
 * @type {Function}
 */
var deferRunTest;

/**
 * Test fixture for testing deferred async tests.
 * @extends {WebUIBrowserAsyncGenTest}
 */
function WebUIBrowserAsyncGenDeferredTest() {}

WebUIBrowserAsyncGenDeferredTest.prototype = {
  __proto__: WebUIBrowserAsyncGenTest.prototype,

  /** @inheritDoc */
  typedefCppFixture: 'WebUIBrowserAsyncGenTest',

  /**
   * True when runTest is called.
   * @type {boolean}
   * @private
   */
  ranTest_: false,

  /** @inheritDoc */
  preLoad: function() {
    deferRunTest = this.deferRunTest(WhenTestDone.DEFAULT);
  },

  /** @inheritDoc */
  setUp: function() {
    continueTest = this.continueTest(WhenTestDone.DEFAULT, function() {
      expectFalse(this.ranTest_);
      chrome.send('callJS', ['deferRunTest']);
    });
    chrome.send('callJS', ['continueTest']);
  },

  /** @inheritDoc */
  tearDown: function() {
    expectTrue(this.ranTest_);
    WebUIBrowserAsyncGenTest.prototype.tearDown.call(this);
  },
};

// Test that the test can be deferred appropriately.
TEST_F('WebUIBrowserAsyncGenDeferredTest', 'TestDeferRunTest', function() {
  this.ranTest_ = true;
});
