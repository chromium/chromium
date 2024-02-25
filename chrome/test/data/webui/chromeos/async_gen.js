// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

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
    assertFalse(this.tornDown);
    assertFalse(this.running);
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
GEN('#include "chrome/test/data/webui/chromeos/async_gen.h"');

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
TEST_F('WebUIBrowserAsyncGenTest', 'TestRunAllActionsAsync', function() {
  // Receive some saved arguments.
  var var3, var4;
  var savedArgs = new SaveMockArguments();
  var savedArgs2 = new SaveMockArguments();
  function testSavedArgs(sendArg) {
    const action1 = callFunctionWithSavedArgs(savedArgs, function(args) {
      var3 = args[0];
    }, sendArg);
    const action2 = callFunctionWithSavedArgs(savedArgs2, function(args) {
      var4 = args[0];
    }, sendArg);
    runAllActionsAsync(WhenTestDone.DEFAULT, action1, action2).invoke();
  }

  // Receive some saved arguments and some bound arguments.
  var var5, var6, var7, var8;
  function testMixedArgs(sendArg) {
    const action1 =
        callFunctionWithSavedArgs(savedArgs, function(passedArgs, boundArgs) {
          var5 = passedArgs[0];
          var6 = boundArgs[0];
        }, sendArg, ['val6']);
    const action2 =
        callFunctionWithSavedArgs(savedArgs2, function(passedArgs, boundArgs) {
          var7 = passedArgs[0];
          var8 = boundArgs[0];
        }, sendArg, ['val8']);
    runAllActionsAsync(WhenTestDone.DEFAULT, action1, action2).invoke();
  }

  // Send the cases to the mocked handler & tell the C++ handler to continue2.
  window.continueTest = this.continueTest(WhenTestDone.ASSERT, function() {
    testSavedArgs(['passedVal1']);
    testMixedArgs(['passedVal2']);
    setTimeout(window.continueTest2, 0);
  });

  // Check expectations after mocks have been called.
  window.continueTest2 = this.continueTest(WhenTestDone.ALWAYS, function() {
    assertEquals('passedVal1', var3);
    assertEquals('passedVal1', var4);
    assertEquals('passedVal2', var5);
    assertEquals('val6', var6);
    assertEquals('passedVal2', var7);
    assertEquals('val8', var8);
  });

  // Kick off the tests asynchronously.
  setTimeout(window.continueTest, 0);
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
      assertFalse(this.ranTest_);
      chrome.send('callJS', ['deferRunTest']);
    });
    chrome.send('callJS', ['continueTest']);
  },

  /** @inheritDoc */
  tearDown: function() {
    assertTrue(this.ranTest_);
    WebUIBrowserAsyncGenTest.prototype.tearDown.call(this);
  },
};

// Test that the test can be deferred appropriately.
TEST_F('WebUIBrowserAsyncGenDeferredTest', 'TestDeferRunTest', function() {
  this.ranTest_ = true;
});
