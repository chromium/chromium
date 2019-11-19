// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Create a mock function that records function calls and validates against
 * expectations.
 */
/* #export */ class MockMethod {
  constructor() {
    var fn = function() {
      var args = Array.prototype.slice.call(arguments);
      var callbacks = args.filter(function(arg) {
        return (typeof arg == 'function');
      });

      if (callbacks.length > 1) {
        console.error(
            'Only support mocking function with at most one callback.');
        return;
      }

      fn.recordCall(args);
      if (callbacks.length == 1) {
        callbacks[0].apply(undefined, fn.callbackData);
        return;
      }
      return fn.returnValue;
    };

    /**
     * List of signatures for function calls.
     * @type {!Array<!Array>}
     * @private
     */
    fn.calls_ = [];

    /**
     * List of expected call signatures.
     * @type {!Array<!Array>}
     * @private
     */
    fn.expectations_ = [];

    /**
     * Value returned from call to function.
     * @type {*}
     */
    fn.returnValue = undefined;

    /**
     * List of arguments for callback function.
     * @type {!Array<!Array>}
     */
    fn.callbackData = [];

    Object.setPrototypeOf(fn, MockMethod.prototype);
    return fn;
  }

  /**
   * Adds an expected call signature.
   * @param {...}  var_args Expected arguments for the function call.
   */
  addExpectation() {
    var args = Array.prototype.slice.call(arguments);
    this.expectations_.push(args.filter(this.notFunction_));
  }

  /**
   * Adds a call signature.
   * @param {!Array} args.
   */
  recordCall(args) {
    this.calls_.push(args.filter(this.notFunction_));
  }

  /**
   * Verifies that the function is called the expected number of times and with
   * the correct signature for each call.
   */
  verifyMock() {
    var errorMessage = 'Number of method calls did not match expectation.';
    if (this.functionName) {
      errorMessage = 'Error in ' + this.functionName + ':\n' + errorMessage;
    }
    assertEquals(this.expectations_.length, this.calls_.length, errorMessage);
    for (var i = 0; i < this.expectations_.length; i++) {
      this.validateCall(i, this.expectations_[i], this.calls_[i]);
    }
  }

  /**
   * Verifies that the observed function arguments match expectations.
   * Override if strict equality is not required.
   * @param {number} index Canonical index of the function call. Unused in the
   *     base implementation, but provides context that may be useful for
   *     overrides.
   * @param {!Array} expected The expected arguments.
   * @parma {!Array} observed The observed arguments.
   */
  validateCall(index, expected, observed) {
    assertDeepEquals(expected, observed);
  }

  /**
   * Test if arg is a function.
   * @param {*} arg The argument to test.
   * @return True if arg is not function type.
   */
  notFunction_(arg) {
    return typeof arg != 'function';
  }
}

/**
 * Controller for mocking methods. Tracks calls to mocked methods and verifies
 * that call signatures match expectations.
 */
/* #export */ class MockController {
  constructor() {
    /**
     * Original functions implementations, which are restored when |reset| is
     * called.
     * @type {!Array<!Object>}
     * @private
     */
    this.overrides_ = [];

    /**
     * List of registered mocks.
     * @type {!Array<!MockMethod>}
     * @private
     */
    this.mocks_ = [];
  }

  /**
   * Creates a mock function.
   * @param {Object=} opt_parent Optional parent object for the function.
   * @param {string=} opt_functionName Optional name of the function being
   *     mocked. If the parent and function name are both provided, the
   *     mock is automatically substituted for the original and replaced on
   *     reset.
   */
  createFunctionMock(opt_parent, opt_functionName) {
    var fn = new MockMethod();

    // Register mock.
    if (opt_parent && opt_functionName) {
      this.overrides_.push({
        parent: opt_parent,
        functionName: opt_functionName,
        originalFunction: opt_parent[opt_functionName]
      });
      opt_parent[opt_functionName] = fn;
      fn.functionName = opt_functionName;
    }
    this.mocks_.push(fn);

    return fn;
  }

  /**
   * Validates all mocked methods. An exception is thrown if the
   * expected and actual calls to a mocked function to not align.
   */
  verifyMocks() {
    for (var i = 0; i < this.mocks_.length; i++) {
      this.mocks_[i].verifyMock();
    }
  }

  /**
   * Discard mocks reestoring default behavior.
   */
  reset() {
    for (var i = 0; i < this.overrides_.length; i++) {
      var override = this.overrides_[i];
      override.parent[override.functionName] = override.originalFunction;
    }
  }
}
