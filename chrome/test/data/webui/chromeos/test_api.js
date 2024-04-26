// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Library providing basic test framework functionality.
 */

/* eslint-disable no-console */

/**
 * See assert.js for where this is used.
 * @suppress {globalThis}
 */
this.traceAssertionsForTesting = true;

/** @suppress {globalThis} */
// eslint-disable-next-line no-var
var hasWindow = !!this.window;

/**
 * Namespace for |Test|.
 * @type {Object}
 */
// eslint-disable-next-line no-var
var testing = {};
(function(exports) {
/**
 * Hold the currentTestCase across between preLoad and run.
 * @type {TestCase}
 */
let currentTestCase = null;

/**
 * Value set to true by WebUIBrowserTest if test harness should wait for user to
 * attach a debugger.
 *
 * @type {boolean}
 */
let waitUser = false;

/**
 * The string representation of the currently running test function.
 * @type {?string}
 */
let currentTestFunction = null;

/**
 * The arguments of the currently running test.
 * @type {Array}
 */
let currentTestArguments = [];

/**
 * This class will be exported as testing.Test, and is provided to hold the
 * fixture's configuration and callback methods for the various phases of
 * invoking a test. It is called "Test" rather than TestFixture to roughly
 * mimic the gtest's class names.
 * @constructor
 */
function Test() {}

/**
 * Make all transitions and animations take 0ms. NOTE: this will completely
 * disable webkitTransitionEnd events. If your code relies on them firing, it
 * will break. animationend events should still work.
 */
Test.disableAnimationsAndTransitions = function() {
  const all = document.body.querySelectorAll('*');
  const ZERO_MS_IMPORTANT = '0ms !important';
  for (let i = 0, l = all.length; i < l; ++i) {
    const style = all[i].style;
    style.animationDelay = ZERO_MS_IMPORTANT;
    style.animationDuration = ZERO_MS_IMPORTANT;
    style.transitionDelay = ZERO_MS_IMPORTANT;
    style.transitionDuration = ZERO_MS_IMPORTANT;
  }

  const realElementAnimate = Element.prototype.animate;
  Element.prototype.animate = function(keyframes, opt_options) {
    if (typeof opt_options === 'object') {
      opt_options.duration = 0;
    } else {
      opt_options = 0;
    }
    return realElementAnimate.call(this, keyframes, opt_options);
  };
  if (document.timeline && document.timeline.play) {
    const realTimelinePlay = document.timeline.play;
    document.timeline.play = function(a) {
      a.timing.duration = 0;
      return realTimelinePlay.call(document.timeline, a);
    };
  }
};

Test.prototype = {
  /**
   * The name of the test.
   */
  name: null,

  /**
   * When set to a string value representing a url, generate BrowsePreload
   * call, which will browse to the url and call fixture.preLoad of the
   * currentTestCase.
   * @type {?string}
   */
  browsePreload: null,

  /** @type {?string} */
  webuiHost: null,

  /**
   * When set to a function, will be called in the context of the test
   * generation inside the function, after AddLibrary calls and before
   * generated C++.
   * @type {?function(string,string)}
   */
  testGenPreamble: null,

  /**
   * When set to a function, will be called in the context of the test
   * generation inside the function, and after any generated C++.
   * @type {?function(string,string)}
   */
  testGenPostamble: null,

  /** @type {?function()} */
  testGenCppIncludes: null,

  /**
   * When set to a non-null string, auto-generate typedef before generating
   * TEST*: {@code typedef typedefCppFixture testFixture}.
   * @type {string}
   */
  typedefCppFixture: 'WebUIBrowserTest',

  /** @type {?Array<{switchName: string, switchValue: string}>} */
  commandLineSwitches: null,

  /** @type {?{enabled: !Array<string>, disabled: !Array<string>}} */
  featureList: null,

  /**
   * @type {?Array<!{
   *    featureName: string,
   *    parameters: !Array<{name: string, value: string}>}>}
   */
  featuresWithParameters: null,

  /**
   * Value is passed through call to C++ RunJavascriptF to invoke this test.
   * @type {boolean}
   */
  isAsync: false,

  /**
   * True when the test is expected to fail for testing the test framework.
   * @type {boolean}
   */
  testShouldFail: false,

  /**
   * Starts a local test server if true and injects the server's base url to
   * each test. The url can be accessed from
   * |testRunnerParams.testServerBaseUrl|.
   * @type {boolean}
   */
  testServer: false,

  /**
   * Extra libraries to add before loading this test file.
   * @type {Array<string>}
   */
  extraLibraries: [],

  /**
   * Extra libraries to add before loading this test file.
   * This list is in the form of Closure library style object
   * names.  To support this, a closure deps.js file must
   * be specified when generating the test C++ source.
   * The specified libraries will be included with their transitive
   * dependencies according to the deps file.
   * @type {Array<string>}
   */
  closureModuleDeps: [],

  /**
   * Override this method to perform initialization during preload (such as
   * creating mocks and registering handlers).
   * @type {Function}
   */
  preLoad: function() {},

  /**
   * Override this method to perform tasks before running your test.
   * @type {Function}
   */
  setUp: function() {},

  /**
   * Override this method to perform tasks after running your test.
   * @type {Function}
   */
  tearDown: function() {
    if (typeof document !== 'undefined') {
      const noAnimationStyle = document.getElementById('no-animation');
      if (noAnimationStyle) {
        noAnimationStyle.parentNode.removeChild(noAnimationStyle);
      }
    }
  },

  /**
   * Called to run the body from the perspective of this fixture.
   * @type {Function}
   */
  runTest: function(testBody) {
    testBody.call(this);
  },

  /**
   * Create a closure function for continuing the test at a later time. May be
   * used as a listener function.
   * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
   *     time.
   * @param {!Function} completion The function to call to complete the test.
   * @param {...*} var_args Arguments to pass when calling completionAction.
   * @return {function(): void} Return a function, bound to this test fixture,
   *     which continues the test.
   */
  continueTest: function(whenTestDone, completion, var_args) {
    const savedArgs = new SaveMockArguments();
    const completionAction = new CallFunctionAction(
        this, savedArgs, completion, Array.prototype.slice.call(arguments, 2));
    if (whenTestDone === WhenTestDone.DEFAULT) {
      whenTestDone = WhenTestDone.ASSERT;
    }
    const runAll = new RunAllAction(true, whenTestDone, [completionAction]);
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  },

  /**
   * Call this during setUp to defer the call to runTest() until later. The
   * caller must call the returned function at some point to run the test.
   * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
   *     time.
   * @param {...*} var_args Arguments to pass when running the
   *     |currentTestCase|.
   * @return {function(): void} A function which will run the current body of
   *     the currentTestCase.
   */
  deferRunTest: function(whenTestDone, var_args) {
    if (whenTestDone === WhenTestDone.DEFAULT) {
      whenTestDone = WhenTestDone.ALWAYS;
    }

    return currentTestCase.deferRunTest.apply(
        currentTestCase,
        [whenTestDone].concat(Array.prototype.slice.call(arguments, 1)));
  },
};

/**
 * This class is not exported and is available to hold the state of the
 * |currentTestCase| throughout preload and test run.
 * @param {string} name The name of the test case.
 * @param {Test} fixture The fixture object for this test case.
 * @param {Function} body The code to run for the test.
 * @constructor
 */
function TestCase(name, fixture, body) {
  this.name = name;
  this.fixture = fixture;
  this.body = body;
}

TestCase.prototype = {
  /**
   * The name of this test.
   * @type {?string}
   */
  name: null,

  /**
   * The test fixture to set |this| to when running the test |body|.
   * @type {Test}
   */
  fixture: null,

  /**
   * The test body to execute in runTest().
   * @type {Function}
   */
  body: null,

  /**
   * True when the test fixture will run the test later.
   * @type {boolean}
   * @private
   */
  deferred_: false,

  /**
   * Called at preload time, proxies to the fixture.
   * @type {Function}
   */
  preLoad: function(name) {
    if (this.fixture) {
      this.fixture.preLoad();
    }
  },

  /**
   * Called before a test runs.
   */
  setUp: function() {
    if (this.fixture) {
      this.fixture.setUp();
    }
  },

  /**
   * Called before a test is torn down (by testDone()).
   */
  tearDown: function() {
    if (this.fixture) {
      this.fixture.tearDown();
    }
  },

  /**
   * Called to run this test's body.
   */
  runTest: function() {
    if (this.body && this.fixture) {
      this.fixture.runTest(this.body);
    }
  },

  /**
   * Runs this test case with |this| set to the |fixture|.
   *
   * Note: Tests created with TEST_F may depend upon |this| being set to an
   * instance of this.fixture. The current implementation of TEST creates a
   * dummy constructor, but tests created with TEST should not rely on |this|
   * being set.
   * @type {Function}
   */
  run: function() {
    try {
      this.setUp();
    } catch (e) {
      // Mock4JSException doesn't inherit from Error, so fall back on
      // toString().
      console.error(e.stack || e.toString());
    }

    if (!this.deferred_) {
      this.runTest();
    }

    // tearDown called by testDone().
  },

  /**
   * Cause this TestCase to be deferred (don't call runTest()) until the
   * returned function is called.
   * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
   *     time.
   * @param {...*} var_args Arguments to pass when running the
   *     |currentTestCase|.
   * @return {function(): void} A function that will run this TestCase when
   *     called.
   */
  deferRunTest: function(whenTestDone, var_args) {
    this.deferred_ = true;
    const savedArgs = new SaveMockArguments();
    const completionAction = new CallFunctionAction(
        this, savedArgs, this.runTest,
        Array.prototype.slice.call(arguments, 1));
    const runAll = new RunAllAction(true, whenTestDone, [completionAction]);
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  },

};

/**
 * true when testDone has been called.
 * @type {boolean}
 */
let testIsDone = false;

/**
 * Holds the errors, if any, caught by expects so that the test case can
 * fail. Cleared when results are reported from runTest() or testDone().
 * @type {Array<Error>}
 */
const errors = [];

/**
 * URL to dummy WebUI page for testing framework.
 * @type {string}
 */
const DUMMY_URL = 'chrome://DummyURL';

/**
 * Resets test state by clearing |errors| and |testIsDone| flags.
 */
function resetTestState() {
  errors.splice(0, errors.length);
  testIsDone = false;
}

/**
 * Notifies the running browser test of the test results. Clears |errors|.
 * No tuple type: b/131114945 (result should be {[boolean, string]}).
 * @param {Array=} result When passed, this is used for the testResult message.
 */
function testDone(result) {
  if (!testIsDone) {
    testIsDone = true;
    if (currentTestCase) {
      let ok = true;
      ok = createExpect(currentTestCase.tearDown.bind(currentTestCase))
               .call(null) &&
          ok;

      if (!ok && result) {
        result = [false, errorsToMessage(errors, result[1])];
      }

      currentTestCase = null;
    }
    if (!result) {
      result = testResult();
    }

    const [success, errorMessage] = /** @type {!Array} */ (result);
    if (hasWindow && window.reportMojoWebUITestResult) {
      // For "mojo_webui" test types, reportMojoWebUITestResult should already
      // be defined globally, because such tests must manually import the
      // mojo_webui_test_support.js module which defines it.
      if (success) {
        window.reportMojoWebUITestResult();
      } else {
        window.reportMojoWebUITestResult(errorMessage);
      }
    } else if (hasWindow && window.webUiTest) {
      let testRunner;
      if (webUiTest.mojom.TestRunnerRemote) {
        // For mojo-lite WebUI tests.
        testRunner = webUiTest.mojom.TestRunner.getRemote();
      } else {
        assertNotReached(
            'Mojo bindings found, but no valid test interface loaded');
      }
      if (success) {
        testRunner.testComplete(null);
      } else {
        testRunner.testComplete(errorMessage);
      }
    } else if (chrome.send) {
      // For WebUI and v8 unit tests.
      chrome.send('testResult', result);
    } else if (chrome.test.sendScriptResult) {
      // For extension tests.
      const valueResult = {'result': success, message: errorMessage};
      chrome.test.sendScriptResult(JSON.stringify(valueResult));
    } else {
      assertNotReached('No test framework available');
    }
    errors.splice(0, errors.length);
  } else {
    console.warn('testIsDone already');
  }
}

/**
 * Converts each Error in |errors| to a suitable message, adding them to
 * |message|, and returns the message string.
 * @param {Array<Error>} errors Array of errors to add to |message|.
 * @param {string=} opt_message Message to append error messages to.
 * @return {string} |opt_message| + messages of all |errors|.
 */
function errorsToMessage(errors, opt_message) {
  let message = '';
  if (opt_message) {
    message += opt_message + '\n';
  }

  for (let i = 0; i < errors.length; ++i) {
    const errorMessage = errors[i].stack || errors[i].message;
    // Cast JSON.stringify to Function to avoid formal parameter mismatch.
    message += 'Failed: ' + currentTestFunction + '(' +
        currentTestArguments.map(/** @type{Function} */ (JSON.stringify)) +
        ')\n' + errorMessage;
  }
  return message;
}

/**
 * Returns [success, message] & clears |errors|.
 * @param {boolean=} errorsOk When true, errors are ok.
 *
 * No tuple type: b/131114945 (result should be {[boolean, string]}).
 * @return {Array}
 */
function testResult(errorsOk) {
  let result = [true, ''];
  if (errors.length) {
    result = [!!errorsOk, errorsToMessage(errors)];
  }

  return result;
}

// Asserts.
// Use the following assertions to verify a condition within a test.

/**
 * @param {boolean} value The value to check.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertTrue(value, opt_message) {
  chai.assert.isTrue(value, opt_message);
}

/**
 * @param {boolean} value The value to check.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertFalse(value, opt_message) {
  chai.assert.isFalse(value, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertGE(value1, value2, opt_message) {
  chai.expect(value1).to.be.at.least(value2, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertGT(value1, value2, opt_message) {
  chai.assert.isAbove(value1, value2, opt_message);
}

/**
 * @param {*} expected The expected value.
 * @param {*} actual The actual value.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertEquals(expected, actual, opt_message) {
  chai.assert.strictEqual(actual, expected, opt_message);
}

/**
 * @param {*} expected
 * @param {*} actual
 * {string=} opt_message
 * @throws {Error}
 */
function assertDeepEquals(expected, actual, opt_message) {
  chai.assert.deepEqual(actual, expected, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertLE(value1, value2, opt_message) {
  chai.expect(value1).to.be.at.most(value2, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertLT(value1, value2, opt_message) {
  chai.assert.isBelow(value1, value2, opt_message);
}

/**
 * @param {*} expected The expected value.
 * @param {*} actual The actual value.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertNotEquals(expected, actual, opt_message) {
  chai.assert.notStrictEqual(actual, expected, opt_message);
}

/**
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertNotReached(opt_message) {
  chai.assert.fail(null, null, opt_message);
}

/**
 * @param {function()} testFunction
 * @param {(Function|string|RegExp)=} opt_expected_or_constructor The expected
 *     Error constructor, partial or complete error message string, or RegExp to
 *     test the error message.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
function assertThrows(testFunction, opt_expected_or_constructor, opt_message) {
  // The implementation of assert.throws goes like:
  //  function (fn, errt, errs, msg) {
  //    if ('string' === typeof errt || errt instanceof RegExp) {
  //      errs = errt;
  //      errt = null;
  //    }
  //    ...
  // That is, if the second argument is string or RegExp, the type of the
  // exception is not checked: only the error message. This is achieved by
  // partially "shifting" parameters (the "additional error message" is not
  // shifted and will be lost). "Shifting" isn't a thing Closure understands, so
  // just cast to string.
  // TODO(crbug.com/40097498): Refactor this into something that makes sense when
  // tests are actually compiled and we can do that safely.
  chai.assert.throws(
      testFunction,
      /** @type{string} */ (opt_expected_or_constructor), opt_message);
}

/**
 * Creates a function based upon a function that throws an exception on
 * failure. The new function stuffs any errors into the |errors| array for
 * checking by runTest. This allows tests to continue running other checks,
 * while failing the overall test if any errors occurred.
 * @param {Function} assertFunc The function which may throw an Error.
 * @return {function(...*):boolean} A function that applies its arguments to
 *     |assertFunc| and returns true if |assertFunc| passes.
 * @see errors
 * @see runTestFunction
 */
function createExpect(assertFunc) {
  return function() {
    try {
      assertFunc.apply(null, arguments);
    } catch (e) {
      errors.push(e);
      return false;
    }
    return true;
  };
}

/**
 * This is the starting point for tests run by WebUIBrowserTest.  If an error
 * occurs, it reports a failure and a message created by joining individual
 * error messages. This supports sync tests and async tests by calling
 * testDone() when |isAsync| is not true, relying on async tests to call
 * testDone() when they complete.
 * @param {boolean} isAsync When false, call testDone() with the test result
 *     otherwise only when assertions are caught.
 * @param {string} testFunction The function name to call.
 * @param {Array} testArguments The arguments to call |testFunction| with.
 * @return {boolean} true always to signal successful execution (but not
 *     necessarily successful results) of this test.
 * @see errors
 * @see runTestFunction
 */
function runTest(isAsync, testFunction, testArguments) {
  // If waiting for user to attach a debugger, retry in 1 second.
  if (waitUser) {
    setTimeout(runTest, 1000, isAsync, testFunction, testArguments);
    return true;
  }

  // Avoid eval() if at all possible, since it will not work on pages
  // that have enabled content-security-policy.
  /** @type {?Function} */
  let testBody = this[testFunction];  // global object -- not a method.
  let testName = testFunction;

  // Depending on how we were called, |this| might not resolve to the global
  // context.
  if (testName === 'RUN_TEST_F' && testBody === undefined) {
    testBody = RUN_TEST_F;
  }

  if (typeof testBody === 'undefined') {
    testBody = /** @type{Function} */ (eval(testFunction));
    testName = testBody.toString();
  }
  if (testBody !== RUN_TEST_F) {
    console.log('Running test ' + testName);
  }

  // Async allow expect errors, but not assert errors.
  const result =
      runTestFunction(testFunction, testBody, testArguments, isAsync);
  if (!isAsync || !result[0]) {
    testDone(result);
  }
  return true;
}

/**
 * This is the guts of WebUIBrowserTest. It runs the test surrounded by an
 * expect to catch Errors. If |errors| is non-empty, it reports a failure and
 * a message by joining |errors|. Consumers can use this to use assert/expect
 * functions asynchronously, but are then responsible for reporting errors to
 * the browser themselves through testDone().
 * @param {string} testFunction The function name to report on failure.
 * @param {Function} testBody The function to call.
 * @param {Array} testArguments The arguments to call |testBody| with.
 * @param {boolean} onlyAssertFails When true, only assertions cause failing
 *     testResult.
 *
 * No tuple type: b/131114945 (result should be {[boolean, string]}).
 * @return {Array} [test-succeeded, message-if-failed]
 * @see createExpect
 * @see testResult
 */
function runTestFunction(
    testFunction, testBody, testArguments, onlyAssertFails) {
  currentTestFunction = testFunction;
  currentTestArguments = testArguments;
  const ok = createExpect(testBody).apply(null, testArguments);
  return testResult(onlyAssertFails && ok);
}

/**
 * Creates a new test case for the given |testFixture| and |testName|. Assumes
 * |testFixture| describes a globally available subclass of type Test.
 * @param {string} testFixture The fixture for this test case.
 * @param {string} testName The name for this test case.
 * @return {TestCase} A newly created TestCase.
 */
function createTestCase(testFixture, testName) {
  const fixtureConstructor = this[testFixture];
  assertTrue(
      !!fixtureConstructor,
      `The testFixture \'${testFixture}\' was not found.`);
  const testBody = fixtureConstructor.testCaseBodies[testName];
  assertTrue(
      !!testBody, `Test \'${testName} was not found in \'${testFixture}\'.`);
  const fixture = new fixtureConstructor();
  fixture.name = testFixture;
  return new TestCase(testName, fixture, testBody);
}

/**
 * Used by WebUIBrowserTest to preload the javascript libraries at the
 * appropriate time for javascript injection into the current page. This
 * creates a test case and calls its preLoad for any early initialization such
 * as registering handlers before the page's javascript runs it's OnLoad
 * method. This is called before the page is loaded.
 * @param {string} testFixture The test fixture name.
 * @param {string} testName The test name.
 */
function preloadJavascriptLibraries(testFixture, testName) {
  currentTestCase = createTestCase(testFixture, testName);
  currentTestCase.preLoad();
}


/**
 * Sets |waitUser| to true so |runTest| function waits for user to attach a
 * debugger.
 */
function setWaitUser() {
  waitUser = true;
  exports.go = () => waitUser = false;
  console.log('Waiting for debugger...');
  console.log('Run: go() in the JS console when you are ready.');
}

/**
 * During generation phase, this outputs; do nothing at runtime.
 */
function GEN() {}

/**
 * During generation phase, this outputs; do nothing at runtime.
 */
function GEN_INCLUDE() {}

/**
 * At runtime, register the testName with its fixture. Stuff the |name| into
 * the |testFixture|'s prototype, if needed, and the |testCaseBodies| into its
 * constructor.
 * @param {string} testFixture The name of the test fixture class.
 * @param {string} testName The name of the test function.
 * @param {Function} testBody The body to execute when running this test.
 * @param {string=} opt_preamble C++ code to be generated before the test. Does
 * nothing here in the runtime phase.
 */
function TEST_F(testFixture, testName, testBody, opt_preamble) {
  const fixtureConstructor = this[testFixture];
  if (!fixtureConstructor.prototype.name) {
    fixtureConstructor.prototype.name = testFixture;
  }
  if (!fixtureConstructor.hasOwnProperty('testCaseBodies')) {
    fixtureConstructor.testCaseBodies = {};
  }
  fixtureConstructor.testCaseBodies[testName] = testBody;
}

/**
 * Similar to TEST_F above but with a mandatory |preamble|.
 * @param {string} preamble C++ code to be generated before the test. Does
 *                 nothing here in the runtime phase.
 * @param {string} testFixture The name of the test fixture class.
 * @param {string} testName The name of the test function.
 * @param {Function} testBody The body to execute when running this test.
 */
function TEST_F_WITH_PREAMBLE(preamble, testFixture, testName, testBody) {
  TEST_F(testFixture, testName, testBody);
}

/**
 * RunJavascriptTestF uses this as the |testFunction| when invoking
 * runTest. If |currentTestCase| is non-null at this point, verify that
 * |testFixture| and |testName| agree with the preloaded values. Create
 * |currentTestCase|, if needed, run it, and clear the |currentTestCase|.
 * @param {string} testFixture The name of the test fixture class.
 * @param {string} testName The name of the test function.
 * @see preloadJavascriptLibraries
 * @see runTest
 */
function RUN_TEST_F(testFixture, testName) {
  if (!currentTestCase) {
    currentTestCase = createTestCase(testFixture, testName);
  }
  assertEquals(currentTestCase.name, testName);
  assertEquals(currentTestCase.fixture.name, testFixture);
  console.log('Running TestCase ' + testFixture + '.' + testName);
  currentTestCase.run();
}

/**
 * This Mock4JS matcher object pushes each |actualArgument| parameter to
 * match() calls onto |args|.
 * @param {Array} args The array to push |actualArgument| onto.
 * @param {Object} realMatcher The real matcher check arguments with.
 * @constructor
 */
function SaveMockArgumentMatcher(args, realMatcher) {
  this.arguments_ = args;
  this.realMatcher_ = realMatcher;
}

SaveMockArgumentMatcher.prototype = {
  /**
   * Holds the arguments to push each |actualArgument| onto.
   * @type {Array}
   * @private
   */
  arguments_: null,

  /**
   * The real Mock4JS matcher object to check arguments with.
   * @type {Object}
   */
  realMatcher_: null,

  /**
   * Pushes |actualArgument| onto |arguments_| and call |realMatcher_|. Clears
   * |arguments_| on non-match.
   * @param {*} actualArgument The argument to match and save.
   * @return {boolean} Result of calling the |realMatcher|.
   */
  argumentMatches: function(actualArgument) {
    this.arguments_.push(actualArgument);
    const match = this.realMatcher_.argumentMatches(actualArgument);
    if (!match) {
      this.arguments_.splice(0, this.arguments_.length);
    }

    return match;
  },

  /**
   * Proxy to |realMatcher_| for description.
   * @return {string} Description of this Mock4JS matcher.
   */
  describe: function() {
    return this.realMatcher_.describe();
  },
};

/**
 * Actions invoked by Mock4JS's "will()" syntax do not receive arguments from
 * the mocked method. This class works with SaveMockArgumentMatcher to save
 * arguments so that the invoked Action can pass arguments through to the
 * invoked function.
 * @constructor
 */
function SaveMockArguments() {
  this.arguments = [];
}

SaveMockArguments.prototype = {
  /**
   * Wraps the |realMatcher| with an object which will push its argument onto
   * |arguments| and call realMatcher.
   * @param {Object} realMatcher A Mock4JS matcher object for this argument.
   * @return {SaveMockArgumentMatcher} A new matcher which will push its
   *     argument onto |arguments|.
   */
  match: function(realMatcher) {
    return new SaveMockArgumentMatcher(this.arguments, realMatcher);
  },

  /**
   * Remember the argument passed to this stub invocation.
   * @type {Array}
   */
  arguments: null,
};

/**
 * CallFunctionAction is provided to allow mocks to have side effects.
 * @param {Object} obj The object to set |this| to when calling |func_|.
 * @param {?SaveMockArguments} savedArgs when non-null, saved arguments are
 *     passed to |func|.
 * @param {!Function} func The function to call.
 * @param {Array=} args Any arguments to pass to func.
 * @constructor
 */
function CallFunctionAction(obj, savedArgs, func, args) {
  /**
   * Set |this| to |obj_| when calling |func_|.
   * @type {?Object}
   */
  this.obj_ = obj;

  /**
   * The SaveMockArguments to hold arguments when invoking |func_|.
   * @type {?SaveMockArguments}
   * @private
   */
  this.savedArgs_ = savedArgs;

  /**
   * The function to call when invoked.
   * @type {!Function}
   * @private
   */
  this.func_ = func;

  /**
   * Arguments to pass to |func_| when invoked.
   * @type {!Array}
   */
  this.args_ = args || [];
}

CallFunctionAction.prototype = {
  /**
   * Accessor for |func_|.
   * @return {Function} The function to invoke.
   */
  get func() {
    return this.func_;
  },

  /**
   * Called by Mock4JS when using .will() to specify actions for stubs() or
   * expects(). Clears |savedArgs_| so it can be reused.
   * @return The results of calling |func_| with the concatenation of
   *     |savedArgs_| and |args_|.
   */
  invoke: function() {
    let prependArgs = [];
    if (this.savedArgs_) {
      prependArgs =
          this.savedArgs_.arguments.splice(0, this.savedArgs_.arguments.length);
    }
    return this.func.apply(this.obj_, prependArgs.concat(this.args_));
  },

  /**
   * Describe this action to Mock4JS.
   * @return {string} A description of this action.
   */
  describe: function() {
    return 'calls the given function with saved arguments and ' + this.args_;
  },
};

/**
 * Syntactic sugar for use with will() on a Mock4JS.Mock.
 * @param {SaveMockArguments} savedArgs Arguments saved with this object
 *     are passed to |func|.
 * @param {!Function} func The function to call when the method is invoked.
 * @param {...*} var_args Arguments to pass when calling func.
 * @return {CallFunctionAction} Action for use in will.
 */
function callFunctionWithSavedArgs(savedArgs, func, var_args) {
  return new CallFunctionAction(
      null, savedArgs, func, Array.prototype.slice.call(arguments, 2));
}

/**
 * When to call testDone().
 * @enum {number}
 */
const WhenTestDone = {
  /**
   * Default for the method called.
   */
  DEFAULT: -1,

  /**
   * Never call testDone().
   */
  NEVER: 0,

  /**
   * Call testDone() on assert failure.
   */
  ASSERT: 1,

  /**
   * Call testDone() if there are any assert or expect failures.
   */
  EXPECT: 2,

  /**
   * Always call testDone().
   */
  ALWAYS: 3,
};

/**
 * Runs all |actions|.
 * @param {boolean} isAsync When true, call testDone() on Errors.
 * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
 *     time.
 * @param {Array<Object>} actions Actions to run.
 * @constructor
 */
function RunAllAction(isAsync, whenTestDone, actions) {
  this.isAsync_ = isAsync;
  this.whenTestDone_ = whenTestDone;
  this.actions_ = actions;
}

RunAllAction.prototype = {
  /**
   * When true, call testDone() on Errors.
   * @type {boolean}
   * @private
   */
  isAsync_: false,

  /**
   * Call testDone() at appropriate time.
   * @type {WhenTestDone}
   * @private
   * @see WhenTestDone
   */
  whenTestDone_: WhenTestDone.ASSERT,

  /**
   * Holds the actions to execute when invoked.
   * @type {Array}
   * @private
   */
  actions_: null,

  /**
   * Runs all |actions_|, returning the last one. When running in sync mode,
   * throws any exceptions to be caught by runTest() or
   * runTestFunction(). Call testDone() according to |whenTestDone_| setting.
   */
  invoke: function() {
    try {
      let result;
      for (let i = 0; i < this.actions_.length; ++i) {
        result = this.actions_[i].invoke();
      }

      if ((this.whenTestDone_ === WhenTestDone.EXPECT && errors.length) ||
          this.whenTestDone_ === WhenTestDone.ALWAYS) {
        testDone();
      }

      return result;
    } catch (e) {
      if (!(e instanceof Error)) {
        e = new Error(e.toString());
      }

      if (!this.isAsync_) {
        throw e;
      }

      errors.push(e);
      if (this.whenTestDone_ !== WhenTestDone.NEVER) {
        testDone();
      }
    }
  },

  /**
   * Describe this action to Mock4JS.
   * @return {string} A description of this action.
   */
  describe: function() {
    return 'Calls all actions: ' + this.actions_;
  },
};

/**
 * Syntactic sugar for use with will() on a Mock4JS.Mock.
 * @param {...*} var_args Actions to run.
 * @return {RunAllAction} Action for use in will.
 */
function runAllActions(var_args) {
  return new RunAllAction(
      false, WhenTestDone.NEVER, Array.prototype.slice.call(arguments));
}

/**
 * Syntactic sugar for use with will() on a Mock4JS.Mock.
 * @param {WhenTestDone} whenTestDone Call testDone() at the appropriate
 *     time.
 * @param {...*} var_args Actions to run.
 * @return {RunAllAction} Action for use in will.
 */
function runAllActionsAsync(whenTestDone, var_args) {
  return new RunAllAction(
      true, whenTestDone, Array.prototype.slice.call(arguments, 1));
}

/**
 * Runs a test isolated from the other test-runner machinery in this file which
 * is mostly for the deprecated js2gtest suites. Designed for running with the
 * newer, better `EvalJs` machinery (rather than chrome.send).
 *
 * @param {string} suite Name of the test suite object on `window`.
 * @param {string} name Test method on the `suite`.
 * @param {string} helper A method on `suite` that takes `name` as a string.
 * @return {string}
 */
async function isolatedTestRunner(suite, name, helper) {
  console.log(`Running ${suite}.${name} with isolatedTestRunner(${helper}).`);
  const testSuite = window[suite];
  try {
    if (helper) {
      await testSuite[helper](name);
    } else {
      await testSuite[name]();
    }
    console.log(`${suite}.${name} ran to completion.`);
    return 'test_completed';
  } catch (/* @type {Error} */ error) {
    let message = 'exception';
    if (typeof error === 'object' && error !== null && error['message']) {
      message = error['message'];
      console.log(error['stack']);
    } else {
      console.log(error);
    }
    console.log(`${suite}.${name} threw: ${message}`, error);
    throw error;
  }
}

/**
 * Exports assertion methods. All assertion methods delegate to the chai.js
 * assertion library.
 */
function exportChaiAsserts() {
  exports.assertTrue = assertTrue;
  exports.assertFalse = assertFalse;
  exports.assertGE = assertGE;
  exports.assertGT = assertGT;
  exports.assertEquals = assertEquals;
  exports.assertDeepEquals = assertDeepEquals;
  exports.assertLE = assertLE;
  exports.assertLT = assertLT;
  exports.assertNotEquals = assertNotEquals;
  exports.assertNotReached = assertNotReached;
  exports.assertThrows = assertThrows;
}

/**
 * Exports methods related to Mock4JS mocking.
 */
function exportMock4JsHelpers() {
  exports.callFunctionWithSavedArgs = callFunctionWithSavedArgs;
  exports.SaveMockArguments = SaveMockArguments;
}

// Exports.
testing.Test = Test;
exports.testDone = testDone;
exportChaiAsserts();
exportMock4JsHelpers();
exports.preloadJavascriptLibraries = preloadJavascriptLibraries;
exports.setWaitUser = setWaitUser;
exports.resetTestState = resetTestState;
exports.runAllActions = runAllActions;
exports.runAllActionsAsync = runAllActionsAsync;
exports.runTest = runTest;
exports.runTestFunction = runTestFunction;
exports.DUMMY_URL = DUMMY_URL;
exports.TEST_F = TEST_F;
exports.TEST_F_WITH_PREAMBLE = TEST_F_WITH_PREAMBLE;
exports.RUNTIME_TEST_F = TEST_F;
exports.GEN = GEN;
exports.GEN_INCLUDE = GEN_INCLUDE;
exports.WhenTestDone = WhenTestDone;
exports.isolatedTestRunner = isolatedTestRunner;
})(this);
