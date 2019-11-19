// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/**
 * @typedef {{resolver: !PromiseResolver,
 *            callCount: number}}
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
   * @param {!Array<string>} methodNames Names of all methods whose calls
   *     need to be tracked.
   */
  constructor(methodNames) {
    /** @private {!Map<string, !MethodData>} */
    this.resolverMap_ = new Map();
    methodNames.forEach(methodName => {
      this.createMethodData_(methodName);
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
    methodData.callCount += 1;
    this.resolverMap_.set(methodName, methodData);
    methodData.resolver.resolve(opt_arg);
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
   * @return {!boolean}
   */
  getCallCount(methodName) {
    return this.getMethodData_(methodName).callCount;
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
        methodName, {resolver: new PromiseResolver(), callCount: 0});
  }
}
