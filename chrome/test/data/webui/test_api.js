// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Library providing basic test framework functionality.
 */

/**
 * See assert.js for where this is used.
 * @suppress {globalThis}
 */
this.traceAssertionsForTesting = true;

/** @suppress {globalThis} */
var hasWindow = !!this.window;

/**
 * Namespace for |Test|.
 * @type {Object}
 */
var testing = {};
(function(exports) {
/**
 * Holds the original version of the |chrome| object.
 */
var originalChrome = null;

/**
 * Hold the currentTestCase across between preLoad and run.
 * @type {TestCase}
 */
var currentTestCase = null;

/**
 * Value set to true by WebUIBrowserTest if test harness should wait for user to
 * attach a debugger.
 *
 * @type {boolean}
 */
var waitUser = false;

/**
 * The string representation of the currently running test function.
 * @type {?string}
 */
var currentTestFunction = null;

/**
 * The arguments of the currently running test.
 * @type {Array}
 */
var currentTestArguments = [];

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
  let all = document.body.querySelectorAll('*, * /deep/ *');
  const ZERO_MS_IMPORTANT = '0ms !important';
  for (let i = 0, l = all.length; i < l; ++i) {
    let style = all[i].style;
    style.animationDelay = ZERO_MS_IMPORTANT;
    style.animationDuration = ZERO_MS_IMPORTANT;
    style.transitionDelay = ZERO_MS_IMPORTANT;
    style.transitionDuration = ZERO_MS_IMPORTANT;
  }

  var realElementAnimate = Element.prototype.animate;
  Element.prototype.animate = function(keyframes, opt_options) {
    if (typeof opt_options == 'object') {
      opt_options.duration = 0;
    } else {
      opt_options = 0;
    }
    return realElementAnimate.call(this, keyframes, opt_options);
  };
  if (document.timeline && document.timeline.play) {
    var realTimelinePlay = document.timeline.play;
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

  /**
   * When set to a string value representing an html page in the test
   * directory, generate BrowsePrintPreload call, which will browse to a url
   * representing the file, cause print, and call fixture.preLoad of the
   * currentTestCase.
   * @type {?string}
   */
  browsePrintPreload: null,

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

  /**
   * When set to a non-null string, auto-generate typedef before generating
   * TEST*: {@code typedef typedefCppFixture testFixture}.
   * @type {string}
   */
  typedefCppFixture: 'WebUIBrowserTest',

  /**
   * This should be initialized by the test fixture and can be referenced
   * during the test run. It holds any mocked handler methods.
   * @type {?Mock}
   */
  mockHandler: null,

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
   * Whether to run the accessibility checks.
   * @type {boolean}
   */
  runAccessibilityChecks: true,

  /**
   * Configuration for the accessibility audit.
   * TODO(crbug/1000989): Enable type checks for axs.
   * @type {Object} an axs.AuditConfiguration
   */
  accessibilityAuditConfig_: null,

  /**
   * Returns the configuration for the accessibility audit, creating it
   * on-demand.
   * TODO(crbug/1000989): Enable type checks for axs.
   * @suppress {undefinedVars|missingProperties}
   * @return {Object} an axs.AuditConfiguration!
   */
  get accessibilityAuditConfig() {
    if (!this.accessibilityAuditConfig_) {
      this.accessibilityAuditConfig_ = new axs.AuditConfiguration();

      this.accessibilityAuditConfig_.showUnsupportedRulesWarning = false;

      this.accessibilityAuditConfig_.auditRulesToIgnore = [
        // The "elements with meaningful background image" accessibility
        // audit (AX_IMAGE_01) does not apply, since Chrome doesn't
        // disable background images in high-contrast mode like some
        // browsers do.
        'elementsWithMeaningfulBackgroundImage',

        // Most WebUI pages are inside an IFrame, so the "web page should
        // have a title that describes topic or purpose" test (AX_TITLE_01)
        // generally does not apply.
        'pageWithoutTitle',

        // TODO(aboxhall): re-enable when crbug.com/267035 is fixed.
        // Until then it's just noise.
        'lowContrastElements',

        // TODO(apacible): re-enable when following issue is fixed.
        // github.com/GoogleChrome/accessibility-developer-tools/issues/251
        'tableHasAppropriateHeaders',

        // TODO(crbug.com/657514): This rule is flaky on Linux/ChromeOS.
        'requiredOwnedAriaRoleMissing',
      ];
    }
    return this.accessibilityAuditConfig_;
  },

  /**
   * Whether to treat accessibility issues (errors or warnings) as test
   * failures. If true, any accessibility issues will cause the test to fail.
   * If false, accessibility issues will cause a console.warn.
   * Off by default to begin with; as we add the ability to suppress false
   * positives, we will transition this to true.
   * @type {boolean}
   */
  accessibilityIssuesAreErrors: false,

  /**
   * Holds any accessibility results found during the accessibility audit.
   * @type {Array<Object>}
   */
  a11yResults_: [],

  /**
   * Gets the list of accessibility errors found during the accessibility
   * audit. Only for use in testing.
   * @return {Array<Object>}
   */
  getAccessibilityResults: function() {
    return this.a11yResults_;
  },

  /**
   * Run accessibility checks after this test completes.
   */
  enableAccessibilityChecks: function() {
    this.runAccessibilityChecks = true;
  },

  /**
   * Don't run accessibility checks after this test completes.
   */
  disableAccessibilityChecks: function() {
    this.runAccessibilityChecks = false;
  },

  /**
   * Create a new class to handle |messageNames|, assign it to
   * |this.mockHandler|, register its messages and return it.
   * @return {Mock} Mock handler class assigned to |this.mockHandler|.
   */
  makeAndRegisterMockHandler: function(messageNames) {
    var MockClass = makeMockClass(messageNames);
    this.mockHandler = mock(MockClass);
    registerMockMessageCallbacks(this.mockHandler, MockClass);
    return this.mockHandler;
  },

  /**
   * Create a container of mocked standalone functions to handle
   * |functionNames|, assign it to |this.mockLocalFunctions| and return it.
   * @param {!Array<string>} functionNames
   * @return {Mock} Mock handler class.
   * @see makeMockFunctions
   */
  makeMockLocalFunctions: function(functionNames) {
    this.mockLocalFunctions = makeMockFunctions(functionNames);
    return this.mockLocalFunctions;
  },

  /**
   * Override this method to perform initialization during preload (such as
   * creating mocks and registering handlers).
   * @type {Function}
   */
  preLoad: function() {},

  /**
   * Override this method to perform tasks before running your test.
   * TODO(crbug/1000989): Enable type checks for axs.
   * @suppress {missingProperties}
   * @type {Function}
   */
  setUp: function() {
    // These should be ignored in many of the web UI tests.
    // user-image-stream and supervised-user-creation-image-stream are
    // streaming video elements used for capturing a user image so they
    // won't have captions and should be ignored everywhere.
    this.accessibilityAuditConfig.ignoreSelectors(
        'videoWithoutCaptions', '.user-image-stream');
    this.accessibilityAuditConfig.ignoreSelectors(
        'videoWithoutCaptions', '.supervised-user-creation-image-stream');
  },

  /**
   * Override this method to perform tasks after running your test. If you
   * create a mock class, you must call Mock4JS.verifyAllMocks() in this
   * phase.
   * @type {Function}
   */
  tearDown: function() {
    if (typeof document != 'undefined') {
      var noAnimationStyle = document.getElementById('no-animation');
      if (noAnimationStyle) {
        noAnimationStyle.parentNode.removeChild(noAnimationStyle);
      }
    }

    Mock4JS.verifyAllMocks();
  },

  /**
   * Called to run the body from the perspective of this fixture.
   * @type {Function}
   */
  runTest: function(testBody) {
    testBody.call(this);
  },

  /**
   * Called to run the accessibility audit from the perspective of this
   * fixture.
   */
  runAccessibilityAudit: function() {
    if (!this.runAccessibilityChecks || typeof document === 'undefined') {
      return;
    }

    var auditConfig = this.accessibilityAuditConfig;
    if (!runAccessibilityAudit(this.a11yResults_, auditConfig)) {
      var report = accessibilityAuditReport(this.a11yResults_);
      if (this.accessibilityIssuesAreErrors) {
        throw new Error(report);
      } else {
        console.warn(report);
      }
    }
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
    var savedArgs = new SaveMockArguments();
    var completionAction = new CallFunctionAction(
        this, savedArgs, completion, Array.prototype.slice.call(arguments, 2));
    if (whenTestDone === WhenTestDone.DEFAULT) {
      whenTestDone = WhenTestDone.ASSERT;
    }
    var runAll = new RunAllAction(true, whenTestDone, [completionAction]);
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
   * Called after a test is run (in testDone) to test accessibility.
   */
  runAccessibilityAudit: function() {
    if (this.fixture) {
      this.fixture.runAccessibilityAudit();
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
    var savedArgs = new SaveMockArguments();
    var completionAction = new CallFunctionAction(
        this, savedArgs, this.runTest,
        Array.prototype.slice.call(arguments, 1));
    var runAll = new RunAllAction(true, whenTestDone, [completionAction]);
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  },

};

/**
 * Registry of javascript-defined callbacks for {@code chrome.send}.
 * @type {Object}
 */
var sendCallbacks = {};

/**
 * Registers the message, object and callback for {@code chrome.send}
 * @param {string} name The name of the message to route to this |callback|.
 * @param {Object} messageHandler Pass as |this| when calling the |callback|.
 * @param {function(...)} callback Called by {@code chrome.send}.
 * @see sendCallbacks
 */
function registerMessageCallback(name, messageHandler, callback) {
  sendCallbacks[name] = [messageHandler, callback];
}

/**
 * Register all methods of {@code mockClass.prototype} with messages of the
 * same name as the method, using the proxy of the |mockObject| as the
 * |messageHandler| when registering.
 * @param {Mock} mockObject The mock to register callbacks against.
 * @param {Function} mockClass Constructor for the mocked class.
 * @see registerMessageCallback
 * @see overrideChrome
 */
function registerMockMessageCallbacks(mockObject, mockClass) {
  if (!deferGlobalOverrides && !originalChrome) {
    overrideChrome();
  }
  var mockProxy = mockObject.proxy();
  for (var func in mockClass.prototype) {
    if (typeof mockClass.prototype[func] === 'function') {
      registerMessageCallback(func, mockProxy, mockProxy[func]);
    }
  }
}

/**
 * When preloading JavaScript libraries, this is true until the
 * DOMContentLoaded event has been received as globals cannot be overridden
 * until the page has loaded its JavaScript.
 * @type {boolean}
 */
var deferGlobalOverrides = false;

/**
 * Empty function for use in making mocks.
 * @const
 */
function emptyFunction() {}

/**
 * Make a mock from the supplied |methodNames| array.
 * @param {Array<string>} methodNames Array of names of methods to mock.
 * @return {Function} Constructor with prototype filled in with methods
 *     matching |methodNames|.
 */
function makeMockClass(methodNames) {
  function MockConstructor() {}
  for (var i = 0; i < methodNames.length; i++) {
    MockConstructor.prototype[methodNames[i]] = emptyFunction;
  }
  return MockConstructor;
}

/**
 * Create a new class to handle |functionNames|, add method 'functions()'
 * that returns a container of standalone functions based on the mock class
 * members, and return it.
 * @return {Mock} Mock handler class.
 */
function makeMockFunctions(functionNames) {
  var MockClass = makeMockClass(functionNames);
  var mockFunctions = mock(MockClass);
  var mockProxy = mockFunctions.proxy();

  mockFunctions.functions_ = {};

  for (var func in MockClass.prototype) {
    if (typeof MockClass.prototype[func] === 'function') {
      mockFunctions.functions_[func] = mockProxy[func].bind(mockProxy);
    }
  }

  mockFunctions.functions = function() {
    return this.functions_;
  };

  return mockFunctions;
}

/**
 * Overrides {@code chrome.send} for routing messages to javascript
 * functions. Also falls back to sending with the original chrome object.
 * @param {string} messageName The message to route.
 */
function send(messageName) {
  var callback = sendCallbacks[messageName];
  if (callback != undefined) {
    callback[1].apply(callback[0], Array.prototype.slice.call(arguments, 1));
  } else {
    this.__proto__.send.apply(this.__proto__, arguments);
  }
}

/**
 * true when testDone has been called.
 * @type {boolean}
 */
var testIsDone = false;

/**
 * Holds the errors, if any, caught by expects so that the test case can
 * fail. Cleared when results are reported from runTest() or testDone().
 * @type {Array<Error>}
 */
var errors = [];

/**
 * URL to dummy WebUI page for testing framework.
 * @type {string}
 */
var DUMMY_URL = 'chrome://DummyURL';

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
      var ok = true;
      ok = createExpect(
               currentTestCase.runAccessibilityAudit.bind(currentTestCase))
               .call(null) &&
          ok;
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

    if (hasWindow && window.webUiTest) {
      let testRunner;
      if (webUiTest.mojom.TestRunnerPtr) {
        // For mojo WebUI tests.
        testRunner = new webUiTest.mojom.TestRunnerPtr();

        /**
         * @suppress {missingProperties} for mojo.makeRequest - internal method
         * declared in mojo/public/js/bindings.js.
         */
        const mojoMakeRequest = () => mojo.makeRequest(testRunner);

        Mojo.bindInterface(
            webUiTest.mojom.TestRunner.name, mojoMakeRequest().handle,
            'context', true);
      } else if (webUiTest.mojom.TestRunnerRemote) {
        // For mojo-lite WebUI tests.
        testRunner = webUiTest.mojom.TestRunner.getRemote(true);
      } else {
        assertNotReached(
            'Mojo bindings found, but no valid test interface loaded');
      }
      if (result[0]) {
        testRunner.testComplete();
      } else {
        testRunner.testComplete(result[1]);
      }
    } else if (chrome.send) {
      // For WebUI and v8 unit tests.
      chrome.send('testResult', result);
    } else if (window.domAutomationController.send) {
      // For extension tests.
      const valueResult = {'result': result[0], message: result[1]};
      window.domAutomationController.send(JSON.stringify(valueResult));
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
  var message = '';
  if (opt_message) {
    message += opt_message + '\n';
  }

  for (var i = 0; i < errors.length; ++i) {
    var errorMessage = errors[i].stack || errors[i].message;
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
  var result = [true, ''];
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
  // TODO(crbug/1000989): Refactor this into something that makes sense when
  // tests are actually compiled and we can do that safely.
  chai.assert.throws(
      testFunction,
      /** @type{string} */ (opt_expected_or_constructor), opt_message);
}

/**
 * Run an accessibility audit on the current page state.
 * TODO(crbug/1000989): Enable type checks for axs.
 * @suppress {checkTypes}
 * @param {Array} a11yResults
 * @param {Object=} opt_config, an axs.AuditConfiguration=
 * @return {boolean} Whether there were any errors or warnings
 * @private
 */
function runAccessibilityAudit(a11yResults, opt_config) {
  var auditResults = axs.Audit.run(opt_config);
  for (var i = 0; i < auditResults.length; i++) {
    var auditResult = auditResults[i];
    if (auditResult.result == axs.constants.AuditResult.FAIL) {
      var auditRule = auditResult.rule;
      // TODO(aboxhall): more useful error messages (sadly non-trivial)
      a11yResults.push(auditResult);
    }
  }
  // TODO(aboxhall): have strict (no errors or warnings) vs non-strict
  // (warnings ok)
  // TODO(aboxhall): some kind of info logging for warnings only??
  return (a11yResults.length == 0);
}

/**
 * Concatenates the accessibility error messages for each result in
 * |a11yResults| and
 * |a11yWarnings| in to an accessibility report, appends it to the given
 * |message| and returns the resulting message string.
 * TODO(crbug/1000989): Enable type checks for axs.
 * @suppress {missingProperties}
 * @param {Array<Object>} a11yResults The list of accessibility results
 * @param {string=} opt_message
 * @return {string} |message| + accessibility report.
 */
function accessibilityAuditReport(a11yResults, opt_message) {
  let message = opt_message ? opt_message + '\n\n' : '\n';
  message += 'Accessibility issues found on ' + window.location.href + '\n';
  message += axs.Audit.createReport(a11yResults);
  return message;
}

/**
 * Asserts that the current page state passes the accessibility audit.
 * @param {Array=} opt_results Array to fill with results, if desired.
 */
function assertAccessibilityOk(opt_results) {
  var a11yResults = opt_results || [];
  var auditConfig = currentTestCase.fixture.accessibilityAuditConfig;
  if (!runAccessibilityAudit(a11yResults, auditConfig)) {
    throw new Error(accessibilityAuditReport(a11yResults));
  }
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
  var testBody = this[testFunction];  // global object -- not a method.
  var testName = testFunction;

  // Depending on how we were called, |this| might not resolve to the global
  // context.
  if (testName == 'RUN_TEST_F' && testBody === undefined) {
    testBody = RUN_TEST_F;
  }

  if (typeof testBody === 'undefined') {
    testBody = /** @type{Function} */ (eval(testFunction));
    testName = testBody.toString();
  }
  if (testBody != RUN_TEST_F) {
    console.log('Running test ' + testName);
  }

  // Async allow expect errors, but not assert errors.
  var result = runTestFunction(testFunction, testBody, testArguments, isAsync);
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
  var ok = createExpect(testBody).apply(null, testArguments);
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
  var fixtureConstructor = this[testFixture];
  assertTrue(
      !!fixtureConstructor,
      `The testFixture \'${testFixture}\' was not found.`);
  var testBody = fixtureConstructor.testCaseBodies[testName];
  assertTrue(
      !!testBody, `Test \'${testName} was not found in \'${testFixture}\'.`);
  var fixture = new fixtureConstructor();
  fixture.name = testFixture;
  return new TestCase(testName, fixture, testBody);
}

/**
 * Overrides the |chrome| object to enable mocking calls to chrome.send().
 */
function overrideChrome() {
  if (originalChrome) {
    console.error('chrome object already overridden');
    return;
  }

  originalChrome = chrome;
  /** @suppress {const|checkTypes} */
  chrome = {
    __proto__: originalChrome,
    send: send,
    originalSend: originalChrome.send.bind(originalChrome),
  };
}

/**
 * Used by WebUIBrowserTest to preload the javascript libraries at the
 * appropriate time for javascript injection into the current page. This
 * creates a test case and calls its preLoad for any early initialization such
 * as registering handlers before the page's javascript runs it's OnLoad
 * method. This is called before the page is loaded, so the |chrome| object is
 * not yet bound and this DOMContentLoaded listener will be called first to
 * override |chrome| in order to route messages registered in |sendCallbacks|.
 * @param {string} testFixture The test fixture name.
 * @param {string} testName The test name.
 * @see sendCallbacks
 */
function preloadJavascriptLibraries(testFixture, testName) {
  deferGlobalOverrides = true;

  // The document seems to change from the point of preloading to the point of
  // events (and doesn't fire), whereas the window does not. Listening to the
  // capture phase allows this event to fire first.
  window.addEventListener('DOMContentLoaded', function() {
    if (chrome.send) {
      overrideChrome();
    }

    // Override globals at load time so they will be defined.
    assertTrue(deferGlobalOverrides);
    deferGlobalOverrides = false;
  }, true);
  currentTestCase = createTestCase(testFixture, testName);
  currentTestCase.preLoad();
}


/**
 * Sets |waitUser| to true so |runTest| function waits for user to attach a
 * debugger.
 */
function setWaitUser() {
  waitUser = true;
  console.log('Waiting for debugger...');
  console.log('Run: go() in the JS console when you are ready.');
}

/**
 * Sets |waitUser| to false, so |runTest| function stops waiting for user and
 * start running the tests.
 */
function go() {
  waitUser = false;
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
 * At runtime, register the testName with a test fixture. Since this method
 * doesn't have a test fixture, create a dummy fixture to hold its |name|
 * and |testCaseBodies|.
 * @param {string} testCaseName The name of the test case.
 * @param {string} testName The name of the test function.
 * @param {Function} testBody The body to execute when running this test.
 */
function TEST(testCaseName, testName, testBody) {
  var fixtureConstructor = this[testCaseName];
  if (fixtureConstructor === undefined) {
    fixtureConstructor = function() {};
    this[testCaseName] = fixtureConstructor;
    fixtureConstructor.prototype = {
      __proto__: Test.prototype,
      name: testCaseName,
    };
    fixtureConstructor.testCaseBodies = {};
  }
  fixtureConstructor.testCaseBodies[testName] = testBody;
}

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
  var fixtureConstructor = this[testFixture];
  if (!fixtureConstructor.prototype.name) {
    fixtureConstructor.prototype.name = testFixture;
  }
  if (fixtureConstructor['testCaseBodies'] === undefined) {
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
    var match = this.realMatcher_.argumentMatches(actualArgument);
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
    var prependArgs = [];
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
  }
};

/**
 * Syntactic sugar for use with will() on a Mock4JS.Mock.
 * @param {!Function} func The function to call when the method is invoked.
 * @param {...*} var_args Arguments to pass when calling func.
 * @return {CallFunctionAction} Action for use in will.
 */
function callFunction(func, var_args) {
  return new CallFunctionAction(
      null, null, func, Array.prototype.slice.call(arguments, 1));
}

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
var WhenTestDone = {
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
      var result;
      for (var i = 0; i < this.actions_.length; ++i) {
        result = this.actions_[i].invoke();
      }

      if ((this.whenTestDone_ == WhenTestDone.EXPECT && errors.length) ||
          this.whenTestDone_ == WhenTestDone.ALWAYS) {
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
      if (this.whenTestDone_ != WhenTestDone.NEVER) {
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
 * Mock4JS matcher object that matches the actual argument and the expected
 * value iff their JSON representations are same.
 * @param {Object} expectedValue
 * @constructor
 */
function MatchJSON(expectedValue) {
  this.expectedValue_ = expectedValue;
}

MatchJSON.prototype = {
  /**
   * Checks that JSON representation of the actual and expected arguments are
   * same.
   * @param {Object} actualArgument The argument to match.
   * @return {boolean} Result of the comparison.
   */
  argumentMatches: function(actualArgument) {
    return JSON.stringify(this.expectedValue_) ===
        JSON.stringify(actualArgument);
  },

  /**
   * Describes the matcher.
   * @return {string} Description of this Mock4JS matcher.
   */
  describe: function() {
    return 'eqJSON(' + JSON.stringify(this.expectedValue_) + ')';
  },
};

/**
 * Builds a MatchJSON argument matcher for a given expected value.
 * @param {Object} expectedValue
 * @return {MatchJSON} Resulting Mock4JS matcher.
 */
function eqJSON(expectedValue) {
  return new MatchJSON(expectedValue);
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
 * Exports expect methods. 'expect*' methods allow tests to run until the end
 * even in the presence of failures.
 */
function exportExpects() {
  exports.expectTrue = createExpect(assertTrue);
  exports.expectFalse = createExpect(assertFalse);
  exports.expectGE = createExpect(assertGE);
  exports.expectGT = createExpect(assertGT);
  exports.expectEquals = createExpect(assertEquals);
  exports.expectDeepEquals = createExpect(assertDeepEquals);
  exports.expectLE = createExpect(assertLE);
  exports.expectLT = createExpect(assertLT);
  exports.expectNotEquals = createExpect(assertNotEquals);
  exports.expectNotReached = createExpect(assertNotReached);
  exports.expectAccessibilityOk = createExpect(assertAccessibilityOk);
  exports.expectThrows = createExpect(assertThrows);
}

/**
 * Exports methods related to Mock4JS mocking.
 */
function exportMock4JsHelpers() {
  exports.callFunction = callFunction;
  exports.callFunctionWithSavedArgs = callFunctionWithSavedArgs;
  exports.eqJSON = eqJSON;
  exports.SaveMockArguments = SaveMockArguments;

  // Import the Mock4JS helpers.
  Mock4JS.addMockSupport(exports);
}

// Exports.
testing.Test = Test;
exports.testDone = testDone;
exportChaiAsserts();
exports.assertAccessibilityOk = assertAccessibilityOk;
exportExpects();
exportMock4JsHelpers();
exports.preloadJavascriptLibraries = preloadJavascriptLibraries;
exports.setWaitUser = setWaitUser;
exports.go = go;
exports.registerMessageCallback = registerMessageCallback;
exports.resetTestState = resetTestState;
exports.runAccessibilityAudit = runAccessibilityAudit;
exports.runAllActions = runAllActions;
exports.runAllActionsAsync = runAllActionsAsync;
exports.runTest = runTest;
exports.runTestFunction = runTestFunction;
exports.DUMMY_URL = DUMMY_URL;
exports.TEST = TEST;
exports.TEST_F = TEST_F;
exports.TEST_F_WITH_PREAMBLE = TEST_F_WITH_PREAMBLE;
exports.RUNTIME_TEST_F = TEST_F;
exports.GEN = GEN;
exports.GEN_INCLUDE = GEN_INCLUDE;
exports.WhenTestDone = WhenTestDone;
})(this);
