// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {RoutineName, RoutineResult, RoutineResultInfo, RoutineRunner, StandardRoutineResult} from './diagnostics_types.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemRoutineController mojo interface.
 *
 * TODO(zentaro): Add support for complex routine results.
 */

export class FakeSystemRoutineController {
  constructor() {
    /** private !Map<!RoutineName, !StandardRoutineResult> */
    this.routineResults_ = new Map();

    /**
     * Controls the delay resolving routines. By default this is 0 and routines
     * resolve immediately, but still asynchronously.
     * @private {number}
     **/
    this.delayTimeMilliseconds_ = 0;

    /**
     * Holds the resolver for the next routine waiting to be resolved. This
     * will be null if no routines are in progress.
     * @private {?PromiseResolver}
     */
    this.resolver_ = null;

    /**
     * Holds the remote that is called on completion.
     * @private {?RoutineRunner}
     */
    this.remote_ = null;

    /**
     * Holds the name of the routine currently running.
     * @private {?RoutineName}
     */
    this.routineName_ = null;
  }

  /*
   * Implements SystemRoutineController.RunRoutine.
   * @param {!RoutineName} routineName
   * @param {!RoutineRunner} remoteRunner
   * @return {!Promise}
   */
  runRoutine(routineName, remoteRunner) {
    this.resolver_ = new PromiseResolver();
    this.remote_ = remoteRunner;
    this.routineName_ = routineName;

    // If there is a positive or zero delay then setup a timer, otherwise
    // the routine will wait until resolveRoutineForTesting() is called.
    if (this.delayTimeMilliseconds_ >= 0) {
      setTimeout(() => {
        this.fireRemoteWithResult_();
      }, this.delayTimeMilliseconds_);
    }

    return this.resolver_.promise;
  }

  /**
   *
   * @param {!RoutineName} routineName
   * @param {!StandardRoutineResult} routineResult
   */
  setFakeStandardRoutineResult(routineName, routineResult) {
    this.routineResults_.set(routineName, routineResult);
  }

  /**
   * Sets how long each routine waits before resolving. Setting to a value
   * >=0 will use a timer to delay resolution automatically. Setting to -1
   * will allow a caller to manually determine when each routine resolves.
   *
   * If set to -1, the caller can call resolveRoutineForTesting() and
   * isRoutineInProgressForTesting() to manually control in unit tests.
   * @param {number} delayMilliseconds
   */
  setDelayTimeInMillisecondsForTesting(delayMilliseconds) {
    assert(delayMilliseconds >= -1);
    this.delayTimeMilliseconds_ = delayMilliseconds;
  }

  /**
   * Resolves a routine that is in progress. The delay time must be set to
   * -1 or this method will assert. It will also assert if there is no
   * routine running. Use isRoutineInProgressForTesting() to determine if
   * a routing is currently in progress.
   * @return {!Promise}
   */
  resolveRoutineForTesting() {
    assert(this.delayTimeMilliseconds_ == -1);
    assert(this.resolver_ != null);
    var promise = this.resolver_.promise;

    this.fireRemoteWithResult_();
    return promise;
  }

  /**
   * Returns true if a routine is in progress. The delay time must be -1
   * otherwise this function will assert.
   * @return {boolean}
   */
  isRoutineInProgressForTesting() {
    assert(this.delayTimeMilliseconds_ == -1);
    return this.resolver_ != null;
  }

  /**
   * Returns the expected result for a running routine.
   * @return {RoutineResultInfo}
   * @private
   */
  getResultInfo_() {
    assert(this.routineName_ != null);
    let result = this.routineResults_.get(this.routineName_);
    if (result == undefined) {
      result = StandardRoutineResult.kErrorExecuting;
    }

    /** @type {!RoutineResult} */
    const fullResult = {
      simpleResult: result,
    };

    /** @type {!RoutineResultInfo} */
    const resultInfo = {
      name: this.routineName_,
      result: fullResult,
    };

    return resultInfo;
  }

  /**
   * Fires the remote callback with the expected result.
   * @private
   */
  fireRemoteWithResult_() {
    this.remote_.onRoutineResult(this.getResultInfo_());

    this.resolver_.resolve();
    this.resolver_ = null;
    this.remote_ = null;
    this.routineName_ = null;
  }
}
