// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineName, RoutineResultInfo, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {ExecutionProgress, ResultStatusItem, RoutineListExecutor} from 'chrome://diagnostics/routine_list_executor.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';

export function fakeRoutineListExecutorTestSuite() {
  /** @type {?FakeSystemRoutineController} */
  let controller = null;

  /** @type {?RoutineListExecutor} */
  let executor = null;

  setup(() => {
    controller = new FakeSystemRoutineController();
    executor = new RoutineListExecutor(controller);
  });

  teardown(() => {
    controller = null;
    executor = null;
  });

  /**
   * Helper function that takes an array of RoutineResultInfo which contains
   * the routine name and expected result, initializes the fake routine runner,
   * then executes and validates the results.
   * @param {!Array<!RoutineResultInfo>} routines
   */
  function runRoutinesAndAssertResults(routines) {
    let expectedCallbacks = [];
    let routineNames = [];
    routines.forEach((routine) => {
      // Set the result into the fake.
      assertNotEquals(undefined, routine);
      assertNotEquals(undefined, routine.result);
      assertNotEquals(undefined, routine.result.simpleResult);
      controller.setFakeStandardRoutineResult(
          routine.name, routine.result.simpleResult);

      // Build the list of routines to run.
      routineNames.push(routine.name);

      // Add the "running" callback to the list.
      let status = new ResultStatusItem(routine.name);
      status.progress = ExecutionProgress.kRunning;
      expectedCallbacks.push(status);

      // Add the "completed" callback to the list.
      status = new ResultStatusItem(routine.name);
      status.progress = ExecutionProgress.kCompleted;
      status.result = routine.result;
      expectedCallbacks.push(status);
    });

    // Create the callback that validates the arguments.
    let upto = 0;

    /** @type {!function(!ResultStatusItem)} */
    let statusCallback = (status) => {
      assertTrue(upto < expectedCallbacks.length);
      assertEquals(expectedCallbacks[upto].routine, status.routine);
      assertEquals(expectedCallbacks[upto].progress, status.progress);
      if (status.progress === ExecutionProgress.kRunning) {
        assertEquals(null, status.result);
      } else {
        assertEquals(
            expectedCallbacks[upto].result.simpleResult,
            status.result.simpleResult);
      }

      upto++;
    };

    return executor.runRoutines(routineNames, statusCallback).then(() => {
      // Ensure that all the callbacks were sent.
      assertEquals(expectedCallbacks.length, upto);
    });
  }

  test('SingleTest', () => {
    /** @type {!Array<!RoutineResultInfo>} */
    const routines = [{
      name: RoutineName.kCpuStress,
      result: {simpleResult: StandardRoutineResult.kTestFailed}
    }];
    return runRoutinesAndAssertResults(routines);
  });

  test('MultipleTests', () => {
    /** @type {!Array<!RoutineResultInfo>} */
    const routines = [
      {
        name: RoutineName.kCpuStress,
        result: {simpleResult: StandardRoutineResult.kTestPassed}
      },
      {
        name: RoutineName.kCpuCache,
        result: {simpleResult: StandardRoutineResult.kTestFailed}
      },
      {
        name: RoutineName.kFloatingPoint,
        result: {simpleResult: StandardRoutineResult.kTestPassed}
      },
      {
        name: RoutineName.kPrimeSearch,
        result: {simpleResult: StandardRoutineResult.kTestFailed}
      }
    ];

    return runRoutinesAndAssertResults(routines);
  });
}
