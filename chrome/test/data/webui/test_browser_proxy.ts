// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

type Constructor<T> = new (...args: any[]) => T;

interface MethodData {
  resolver: PromiseResolver<any>;
  args: any[];
  resultMapper?: Function;
}

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
   * Creates a |TestBrowserProxy|, which has mock functions for all functions of
   * class |clazz|.
   */
  static fromClass<T>(clazz: Constructor<T>): (T&TestBrowserProxy) {
    const methodNames = Object.getOwnPropertyNames(clazz.prototype)
                            .filter(methodName => methodName !== 'constructor');
    const proxy = new TestBrowserProxy(methodNames);
    proxy.mockMethods_(methodNames);
    return proxy as unknown as (T&TestBrowserProxy);
  }

  /**
   * Creates a mock implementation for each method name. These mocks allow tests
   * to either set a result when the mock is called using
   * |setResultFor(methodName)|, or set a result mapper function that will be
   * invoked when a method is called using |setResultMapperFor(methodName)|.
   */
  private mockMethods_(methodNames: string[]) {
    methodNames.forEach(methodName => {
      (this as unknown as {[key: string]: Function})[methodName] =
          (...args: any[]) => this.methodCalled(methodName, ...args);
    });
  }

  /**
   * Called by subclasses when a tracked method is called from the code that
   * is being tested.
   * @param args Arguments to be forwarded to the testing code, useful for
   *     checking whether the proxy method was called with the expected
   *     arguments.
   * @return If set the result registered via |setResult[Mapper]For|.
   */
  methodCalled(methodName: string, ...args: any[]): any {
    const methodData = this.resolverMap_.get(methodName);
    assert(methodData);
    const storedArgs = args.length === 1 ? args[0] : args;
    methodData.args.push(storedArgs);
    this.resolverMap_.set(methodName, methodData);
    methodData.resolver.resolve(storedArgs);
    if (methodData.resultMapper) {
      return methodData.resultMapper(...args);
    }
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
   * Sets a function |resultMapper| that is called with the original arguments
   * passed to method named |methodName|. This allows a test to return a unique
   * object each method invovation or have the returned value be different based
   * on the arguments.
   */
  setResultMapperFor(methodName: string, resultMapper: Function) {
    this.getMethodData_(methodName).resultMapper = resultMapper;
  }

  /**
   * Sets the return value of a method.
   */
  setResultFor(methodName: string, value: any) {
    this.getMethodData_(methodName).resultMapper = () => value;
  }

  /**
   * Try to give programmers help with mistyped methodNames.
   */
  private getMethodData_(methodName: string): MethodData {
    // Tip: check that the |methodName| is being passed to |this.constructor|.
    const methodData = this.resolverMap_.get(methodName);
    assert(
        methodData, `Method '${methodName}' not found in TestBrowserProxy.`);
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
