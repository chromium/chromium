// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Assertion helper functions wrapping the chaijs API. */

import {assert, expect} from '../chai.js';

/**
 * @param {boolean} value The value to check.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertTrue(value, opt_message) {
  assert.isTrue(value, opt_message);
}

/**
 * @param {boolean} value The value to check.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertFalse(value, opt_message) {
  assert.isFalse(value, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertGE(value1, value2, opt_message) {
  expect(value1).to.be.at.least(value2, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertGT(value1, value2, opt_message) {
  assert.isAbove(value1, value2, opt_message);
}

/**
 * @param {*} expected The expected value.
 * @param {*} actual The actual value.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertEquals(expected, actual, opt_message) {
  assert.strictEqual(actual, expected, opt_message);
}

/**
 * @param {*} expected
 * @param {*} actual
 * @param {string=} opt_message
 * @throws {Error}
 */
export function assertDeepEquals(expected, actual, opt_message) {
  assert.deepEqual(actual, expected, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertLE(value1, value2, opt_message) {
  expect(value1).to.be.at.most(value2, opt_message);
}

/**
 * @param {number} value1 The first operand.
 * @param {number} value2 The second operand.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertLT(value1, value2, opt_message) {
  assert.isBelow(value1, value2, opt_message);
}

/**
 * @param {*} expected The expected value.
 * @param {*} actual The actual value.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertNotEquals(expected, actual, opt_message) {
  assert.notStrictEqual(actual, expected, opt_message);
}

/**
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertNotReached(opt_message) {
  assert.fail(null, null, opt_message);
}

/**
 * @param {function()} testFunction
 * @param {(Function|string|RegExp)=} opt_expected_or_constructor The expected
 *     Error constructor, partial or complete error message string, or RegExp to
 *     test the error message.
 * @param {string=} opt_message Additional error message.
 * @throws {Error}
 */
export function assertThrows(
    testFunction, opt_expected_or_constructor, opt_message) {
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
  assert.throws(
      testFunction,
      /** @type{string} */ (opt_expected_or_constructor), opt_message);
}

/**
 * Verifies that the contents of the expected and observed arrays match.
 * @param {!Array} expected The expected result.
 * @param {!Array} actual The actual result.
 */
export function assertArrayEquals(expected, actual) {
  assertDeepEquals(expected, actual);
}
