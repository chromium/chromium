// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {RoutineResult, RoutineResultInfo, RoutineRunner, RoutineType, SystemRoutineControllerInterface} from './diagnostics_types.js';

/**
 * Represents the execution progress of a test routine.
 * @enum {number}
 */
export let ExecutionProgress = {
  kNotStarted: 0,
  kRunning: 1,
  kCompleted: 2,
  kCancelled: 3,
};

/**
 * Represents the input to a single routine-result-entry in a
 * routine-result-list.
 */
export class ResultStatusItem {
  constructor(routine) {
    /** @type {!RoutineType} */
    this.routine = routine;

    /** @type {!ExecutionProgress} */
    this.progress = ExecutionProgress.kNotStarted;

    /** @type {?RoutineResult} */
    this.result = null;
  }
};

/**
 * The type of the status callback function.
 * @typedef {!function(!ResultStatusItem)}
 */
export let StatusCallbackFunction;

/**
 * Implements the RoutineRunner remote. Creates a resolver and resolves it when
 * the onRoutineResult function is called.
 */
class ExecutionContext {
  constructor() {
    /** @private {!PromiseResolver} */
    this.resolver_ = new PromiseResolver();

    this.routineRunner = new chromeos.diagnostics.mojom.RoutineRunnerReceiver(
        /** @type {!chromeos.diagnostics.mojom.RoutineRunnerInterface} */ (
            this));
  }

  /**
   * Implements RoutineRunner.onRoutineResult.
   * @param {!RoutineResultInfo} result
   **/
  onRoutineResult(result) {
    this.resolver_.resolve(result);
    this.close();
  }

  whenComplete() {
    return this.resolver_.promise;
  }

  close() {
    this.routineRunner.$.close();
  }
}

/**
 * Executes a list of test routines, firing a status callback with a
 * ResultStatusItem before and after each test. The resulting ResultStatusItem
 * maps directly to an entry in a routine-result-list.
 */
export class RoutineListExecutor {
  /**
   * @param {!SystemRoutineControllerInterface} routineController
   */
  constructor(routineController) {
    /** @private {!SystemRoutineControllerInterface} */
    this.routineController_ = routineController;

    /** @private {?ExecutionContext} */
    this.currentExecutionContext_ = null;
  }

  /*
   * Executes a list of routines providing a status callback as each test
   * starts and finishes. The return promise will resolve when all tests are
   * completed.
   * @param {!Array<!RoutineType>} routines
   * @type {!function(!ResultStatusItem)} statusCallback
   * @param {!Promise}
   */
  runRoutines(routines, statusCallback) {
    assert(routines.length > 0);

    // Create a chain of promises that each schedule the next routine when
    // they complete, firing the status callback before and after each test.
    let promise = Promise.resolve();
    routines.forEach((name) => {
      promise = promise.then(() => {
        // Notify the status callback that a test started running.
        const status = new ResultStatusItem(name);
        status.progress = ExecutionProgress.kRunning;
        statusCallback(status);

        this.currentExecutionContext_ = new ExecutionContext();
        // Create a new remote and execute the next test.
        this.routineController_.runRoutine(
            name,
            this.currentExecutionContext_.routineRunner.$
                .bindNewPipeAndPassRemote());

        // When the test completes, notify the status callback of the
        // result.
        return this.currentExecutionContext_.whenComplete().then((info) => {
          assert(info.type === name);
          const status = new ResultStatusItem(name);
          status.progress = ExecutionProgress.kCompleted;
          status.result = info.result;
          statusCallback(status);
        });
      });
    });

    return promise;
  }

  close() {
    if (this.currentExecutionContext_) {
      this.currentExecutionContext_.close();
    }
  }
}
