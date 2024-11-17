// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';

/**
 * A base class for all test browser proxies to inherit from. Provides helper
 * methods for allowing tests to track when a method was called.
 * TestBrowserProxy should only be used for BrowserProxy objects. Subclasses of
 * TestBrowserProxy should always implement an interface corresponding to the
 * real BrowserProxy they are replacing, i.e.:
 *
 * class MyTestBrowserProxy extends TestBrowserProxy implements MyBrowserProxy
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

interface MethodData {
  resolver: PromiseResolver<any>;
  args: any[];
}

export class TestBrowserProxy {
  private resolverMap_: Map<string, MethodData>;

  /**
   * @param methodNames Names of all methods whose calls need to be tracked.
   */
  constructor(methodNames: string[] = []) {
    this.resolverMap_ = new Map();
    methodNames.forEach(methodName => {
      this.createMethodData_(methodName);
    });
  }

  /**
   * Called by subclasses when a tracked method is called from the code that
   * is being tested.
   * @param args Arguments to be forwarded to the testing code, useful for
   *     checking whether the proxy method was called with the expected
   *     arguments.
   */
  methodCalled(methodName: string, ...args: any[]): void {
    // Tip: check that the |methodName| is being passed to |this.constructor|.
    const methodData = this.resolverMap_.get(methodName);
    assert(methodData, `Method '${methodName}' not found in TestBrowserProxy.`);
    const storedArgs = args.length === 1 ? args[0] : args;
    methodData.args.push(storedArgs);
    this.resolverMap_.set(methodName, methodData);
    methodData.resolver.resolve(storedArgs);
  }

  /**
   * @return A promise that is resolved when the given method is called.
   */
  whenCalled(methodName: string): Promise<any> {
    return this.getMethodData_(methodName).resolver.promise;
  }

  /**
   * Resets the PromiseResolver associated with the given method.
   */
  resetResolver(methodName: string) {
    this.getMethodData_(methodName);
    this.createMethodData_(methodName);
  }

  /**
   * Resets all PromiseResolvers.
   */
  reset() {
    this.resolverMap_.forEach((_value, methodName) => {
      this.createMethodData_(methodName);
    });
  }

  /**
   * Get number of times method is called.
   */
  getCallCount(methodName: string): number {
    return this.getMethodData_(methodName).args.length;
  }

  /**
   * Returns the arguments of calls made to |method|.
   */
  getArgs(methodName: string): any[] {
    return this.getMethodData_(methodName).args;
  }

  /**
   * Try to give programmers help with mistyped methodNames.
   */
  private getMethodData_(methodName: string): MethodData {
    // Tip: check that the |methodName| is being passed to |this.constructor|.
    const methodData = this.resolverMap_.get(methodName);
    assert(methodData, `Method '${methodName}' not found in TestBrowserProxy.`);
    return methodData;
  }

  /**
   * Creates a new |MethodData| for |methodName|.
   */
  private createMethodData_(methodName: string) {
    this.resolverMap_.set(
        methodName, {resolver: new PromiseResolver(), args: []});
  }
}
