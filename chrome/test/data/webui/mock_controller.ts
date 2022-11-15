// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals} from './chai_assert.js';

/**
 * Create a mock function that records function calls and validates against
 * expectations.
 */
export class MockMethod {
  /** List of signatures for function calls. */
  private calls_: any[][] = [];

  /** List of expected call signatures. */
  private expectations_: any[][] = [];

  /** Value returned from call to function. */
  returnValue: any|undefined = undefined;

  /** List of arguments for callback function. */
  callbackData: any[][] = [];

  /** Name of the function being replaced. */
  functionName: string|null = null;

  constructor() {
    const fn: MockMethod|Function = function() {
      const args = Array.prototype.slice.call(arguments);
      const callbacks = args.filter(function(arg) {
        return (typeof arg === 'function');
      });

      if (callbacks.length > 1) {
        console.error(
            'Only support mocking function with at most one callback.');
        return;
      }

      const fnAsMethod = fn as unknown as MockMethod;
      fnAsMethod.recordCall(args);
      if (callbacks.length === 1) {
        callbacks[0].apply(undefined, fnAsMethod.callbackData);
        return;
      }
      return fnAsMethod.returnValue;
    };

    const fnAsMethod = fn as unknown as MockMethod;
    Object.assign(fnAsMethod, this);
    Object.setPrototypeOf(fnAsMethod, MockMethod.prototype);
    return fnAsMethod;
  }

  /**
   * Adds an expected call signature.
   * @param args Expected arguments for the function call.
   */
  addExpectation(...args: any[]) {
    this.expectations_.push(args.filter(this.notFunction_));
  }

  /**
   * Adds a call signature.
   */
  recordCall(args: any[]) {
    this.calls_.push(args.filter(this.notFunction_));
  }

  /**
   * Verifies that the function is called the expected number of times and with
   * the correct signature for each call.
   */
  verifyMock() {
    let errorMessage = 'Number of method calls did not match expectation.';
    if (this.functionName) {
      errorMessage = 'Error in ' + this.functionName + ':\n' + errorMessage;
    }
    assertEquals(this.expectations_.length, this.calls_.length, errorMessage);
    for (let i = 0; i < this.expectations_.length; i++) {
      this.validateCall(i, this.expectations_[i]!, this.calls_[i]!);
    }
  }

  /**
   * Verifies that the observed function arguments match expectations.
   * Override if strict equality is not required.
   * @param index Canonical index of the function call. Unused in the
   *     base implementation, but provides context that may be useful for
   *     overrides.
   * @param expected The expected arguments.
   * @param observed The observed arguments.
   */
  validateCall(_index: number, expected: any[], observed: any[]) {
    assertDeepEquals(expected, observed);
  }

  /**
   * Test if arg is a function.
   * @param arg The argument to test.
   * @return True if arg is not function type.
   */
  private notFunction_(arg: any): boolean {
    return typeof arg !== 'function';
  }
}

interface Override {
  parent: {[key: string]: any};
  functionName: string;
  originalFunction: Function;
}

/**
 * Controller for mocking methods. Tracks calls to mocked methods and verifies
 * that call signatures match expectations.
 */
export class MockController {
  /**
   * Original functions implementations, which are restored when |reset| is
   * called.
   */
  private overrides_: Override[] = [];

  /** List of registered mocks. */
  private mocks_: MockMethod[] = [];

  /**
   * Creates a mock function.
   * @param parent Optional parent object for the function.
   * @param functionName Optional name of the function being
   *     mocked. If the parent and function name are both provided, the
   *     mock is automatically substituted for the original and replaced on
   *     reset.
   */
  createFunctionMock(parent?: {[key: string]: any}, functionName?: string) {
    const fn = new MockMethod();

    // Register mock.
    if (parent && functionName) {
      this.overrides_.push({
        parent: parent,
        functionName: functionName,
        originalFunction: parent[functionName],
      });
      parent[functionName] = fn;
      fn.functionName = functionName;
    }
    this.mocks_.push(fn);

    return fn;
  }

  /**
   * Validates all mocked methods. An exception is thrown if the
   * expected and actual calls to a mocked function to not align.
   */
  verifyMocks() {
    for (let i = 0; i < this.mocks_.length; i++) {
      this.mocks_[i]!.verifyMock();
    }
  }

  /**
   * Discard mocks reestoring default behavior.
   */
  reset() {
    for (let i = 0; i < this.overrides_.length; i++) {
      const override = this.overrides_[i]!;
      override.parent[override.functionName] = override.originalFunction;
    }
  }
}
