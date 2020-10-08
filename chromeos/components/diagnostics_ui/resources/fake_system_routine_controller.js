// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  }

  /*
   * Implements SystemRoutineController.RunRoutine.
   * @param {!RoutineName} routineName
   * @param {!RoutineRunner} remoteRunner
   * @return {!Promise}
   */
  runRoutine(routineName, remoteRunner) {
    return new Promise((resolve) => {
      let result = this.routineResults_.get(routineName);
      if (result == undefined) {
        result = StandardRoutineResult.kErrorExecuting;
      }

      /** @type {!RoutineResult} */
      const fullResult = {
        simple_result: result,
      };

      /** @type {!RoutineResultInfo} */
      const resultInfo = {
        name: routineName,
        result: fullResult,
      };

      remoteRunner.onRoutineResult(resultInfo);
      resolve();
    });
  }

  /**
   *
   * @param {!RoutineName} routineName
   * @param {!StandardRoutineResult} routineResult
   * @private
   */
  setFakeStandardRoutineResult(routineName, routineResult) {
    this.routineResults_.set(routineName, routineResult);
  }
}
