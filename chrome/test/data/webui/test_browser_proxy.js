// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/**
 * @typedef {{resolver: !PromiseResolver,
 *            args: !Array<*>,
 *            resultMapper: (!Function|undefined)}}
 */
let MethodData;

/**
 * A base class for all test browser proxies to inherit from. Provides helper
 * methods for allowing tests to track when a method was called.
 *
 * Subclasses are responsible for calling |methodCalled|, when a method is
 * called, which will trigger callers of |whenCalled| to get notified.
 * For example:
 * --------------------------------------------------------------------------
 * class MyTestBrowserProxy extends TestBrowserProxy {
 *   constructor() {
 *     super(['myMethod']);
 *   }
 *
 *   myMethod(someId) {
 *     this.methodCalled('myMethod', someId);
 *   }
 * };
 *
 * // Test code sample
 *
 * var testBrowserProxy = new MyTestBrowserProxy();
 * // ...Replacing real proxy with test proxy....
 * simulateClickFooButton();
 * testBrowserProxy.whenCalled('fooMethod').then(function(id) {
 *   assertEquals(EXPECTED_ID, id);
 * });
 * --------------------------------------------------------------------------
 */
/* #export */ class TestBrowserProxy {
  /**
   * @param {!Array<string>=} methodNames Names of all methods whose calls
   *     need to be tracked.
   */
  constructor(methodNames = []) {
    /** @private {!Map<string, !MethodData>} */
    this.resolverMap_ = new Map();
    methodNames.forEach(methodName => {
      this.createMethodData_(methodName);
    });
  }

  /**
   * Creates a |TestBrowserProxy|, which has mock functions for all functions of
   * class |clazz|.
   * @param {Object} clazz
   * @return {TestBrowserProxy}
   */
  static fromClass(clazz) {
    const methodNames = Object.getOwnPropertyNames(clazz.prototype)
                            .filter(methodName => methodName !== 'constructor');
    const proxy = new TestBrowserProxy();
    proxy.mockMethods(methodNames);
    return proxy;
  }

  /**
   * Creates a mock implementation for each method name. These mocks allow tests
   * to either set a result when the mock is called using
   * |setResultFor(methodName)|, or set a result mapper function that will be
   * invoked when a method is called using |setResultMapperFor(methodName)|.
   * @param {!Array<string>} methodNames
   * @protected
   * @suppress {checkTypes}
   */
  mockMethods(methodNames) {
    methodNames.forEach(methodName => {
      if (!this.resolverMap_.has(methodName)) {
        this.createMethodData_(methodName);
      }
      this[methodName] = function() {
        const args = Array.from(arguments);
        const argObject = {};
        if (args.length > 1) {
          argObject.args = args;
        } else {
          argObject.arg = args[0];
        }
        return this.methodCalledWithResult_(methodName, argObject);
      }.bind(this);
    });
  }

  /**
   * Called by subclasses when a tracked method is called from the code that
   * is being tested.
   * @param {string} methodName
   * @param {*=} opt_arg Optional argument to be forwarded to the testing
   *     code, useful for checking whether the proxy method was called with
   *     the expected arguments.
   * @protected
   */
  methodCalled(methodName, opt_arg) {
    const methodData = this.resolverMap_.get(methodName);
    methodData.args.push(opt_arg);
    this.resolverMap_.set(methodName, methodData);
    methodData.resolver.resolve(opt_arg);
  }

  /**
   * Called by subclasses when a tracked method is called from the code that
   * is being tested.
   * @param {string} methodName
   * @param {!{arg: *, args: (!Array|undefined)}} argObject Optional argument to
   *     be forwarded to the testing code, useful for checking whether the proxy
   *     method was called with the expected arguments. Only |arg| or |args|
   *     should be set.
   * @return {*}
   * @private
   */
  methodCalledWithResult_(methodName, {arg, args}) {
    assert(arg === undefined || args === undefined);
    this.methodCalled(methodName, args === undefined ? arg : args);
    const {resultMapper} = this.resolverMap_.get(methodName);
    if (resultMapper) {
      assert(typeof resultMapper === 'function');
      if (args !== undefined) {
        return resultMapper(...args);
      }
      return resultMapper(arg);
    }
  }

  /**
   * @param {string} methodName
   * @return {!Promise} A promise that is resolved when the given method
   *     is called.
   */
  whenCalled(methodName) {
    return this.getMethodData_(methodName).resolver.promise;
  }

  /**
   * Resets the PromiseResolver associated with the given method.
   * @param {string} methodName
   */
  resetResolver(methodName) {
    this.getMethodData_(methodName);
    this.createMethodData_(methodName);
  }

  /**
   * Resets all PromiseResolvers.
   */
  reset() {
    this.resolverMap_.forEach((value, methodName) => {
      this.createMethodData_(methodName);
    });
  }

  /**
   * Get number of times method is called.
   * @param {string} methodName
   * @return {number}
   */
  getCallCount(methodName) {
    return this.getMethodData_(methodName).args.length;
  }

  /**
   * Returns the arguments of calls made to |method|.
   * @param {string} methodName
   * @return {!Array<*>}
   */
  getArgs(methodName) {
    return this.getMethodData_(methodName).args;
  }

  /**
   * Sets a function |resultMapper| that is called with the original arguments
   * passed to method named |methodName|. This allows a test to return a unique
   * object each method invovation or have the returned value be different based
   * on the arguments.
   * @param {string} methodName
   * @param {!Function} resultMapper
   */
  setResultMapperFor(methodName, resultMapper) {
    this.getMethodData_(methodName).resultMapper = resultMapper;
  }

  /**
   * Sets the return value of a method.
   * @param {string} methodName
   * @param {*} value
   */
  setResultFor(methodName, value) {
    this.getMethodData_(methodName).resultMapper = () => value;
  }

  /**
   * Try to give programmers help with mistyped methodNames.
   * @param {string} methodName
   * @return {!MethodData}
   * @private
   */
  getMethodData_(methodName) {
    // Tip: check that the |methodName| is being passed to |this.constructor|.
    const methodData = this.resolverMap_.get(methodName);
    assert(
        !!methodData, `Method '${methodName}' not found in TestBrowserProxy.`);
    return methodData;
  }

  /**
   * Creates a new |MethodData| for |methodName|.
   * @param {string} methodName
   * @private
   */
  createMethodData_(methodName) {
    this.resolverMap_.set(
        methodName, {resolver: new PromiseResolver(), args: []});
  }
}
