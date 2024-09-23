// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';

type Constructor<T> = new (...args: any[]) => T;

interface MethodData {
  resolver: PromiseResolver<any>;
  args: any[];
  resultMapper?: Function;
}

/**
 * A base class for all test mocks to inherit from. Provides helper
 * methods for allowing tests to track when a method was called.
 *
 * Must pass a base class T for the mock.
 */
export class TestMock<T> {
  private resolverMap_: Map<keyof T, MethodData>;

  /**
   * @param methodNames Names of all methods whose calls need to be tracked.
   */
  private constructor(methodNames: Array<keyof T> = []) {
    this.resolverMap_ = new Map();
    methodNames.forEach(methodName => {
      this.createMethodData_(methodName);
    });
  }

  /**
   * Creates a |TestMock|, which has mock functions for all functions of
   * class |clazz|.
   */
  static fromClass<T>(clazz: Constructor<T>): (T&TestMock<T>) {
    const methodNames =
        Object.getOwnPropertyNames(clazz.prototype)
            .filter(methodName => methodName !== 'constructor') as
        Array<keyof T>;
    const proxy = new TestMock<T>(methodNames);
    proxy.mockMethods_(methodNames, clazz);
    return proxy as unknown as (T & TestMock<T>);
  }

  /**
   * Creates a mock implementation for each method name. These mocks allow tests
   * to either set a result when the mock is called using
   * |setResultFor(methodName)|, or set a result mapper function that will be
   * invoked when a method is called using |setResultMapperFor(methodName)|.
   */
  private mockMethods_(methodNames: Array<keyof T>, clazz: Constructor<T>) {
    methodNames.forEach(methodName => {
      const descriptor =
          Object.getOwnPropertyDescriptor(clazz.prototype, methodName)!;
      const mockedMethod = (...args: any[]) =>
          this.methodCalled(methodName, ...args);
      if (descriptor.get) {
        descriptor.get = mockedMethod;
      }
      if (descriptor.set) {
        descriptor.set = mockedMethod;
      }
      if (descriptor.value && descriptor.value instanceof Function) {
        descriptor.value = mockedMethod;
      }
      Object.defineProperty(this, methodName, descriptor);
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
  methodCalled(methodName: keyof T, ...args: any[]): any {
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
  whenCalled(methodName: keyof T): Promise<any> {
    return this.getMethodData_(methodName).resolver.promise;
  }

  /**
   * Resets the PromiseResolver associated with the given method.
   */
  resetResolver(methodName: keyof T) {
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
  getCallCount(methodName: keyof T): number {
    return this.getMethodData_(methodName).args.length;
  }

  /**
   * Returns the arguments of calls made to |method|.
   */
  getArgs(methodName: keyof T): any[] {
    return this.getMethodData_(methodName).args;
  }

  /**
   * Sets a function |resultMapper| that is called with the original arguments
   * passed to method named |methodName|. This allows a test to return a unique
   * object each method invovation or have the returned value be different based
   * on the arguments.
   */
  setResultMapperFor(methodName: keyof T, resultMapper: Function) {
    this.getMethodData_(methodName).resultMapper = resultMapper;
  }

  /**
   * Sets the return value of a method.
   */
  setResultFor(methodName: keyof T, value: any) {
    this.getMethodData_(methodName).resultMapper = () => value;
  }

  /**
   * Try to give programmers help with mistyped methodNames.
   */
  private getMethodData_(methodName: keyof T): MethodData {
    // Tip: check that the |methodName| is being passed to |this.constructor|.
    const methodData = this.resolverMap_.get(methodName);
    assert(methodData, `Method '${String(methodName)}' not found in TestMock.`);
    return methodData;
  }

  /**
   * Creates a new |MethodData| for |methodName|.
   */
  private createMethodData_(methodName: keyof T) {
    this.resolverMap_.set(
        methodName, {resolver: new PromiseResolver(), args: []});
  }
}
