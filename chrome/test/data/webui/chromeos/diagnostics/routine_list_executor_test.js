// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PowerRoutineResult, RoutineResultInfo, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
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
    let routineTypes = [];
    routines.forEach((routine) => {
      // Set the result into the fake.
      assertNotEquals(undefined, routine);
      assertNotEquals(undefined, routine.result);

      // simpleResult or powerResult must exist.
      assertTrue(
          routine.result.hasOwnProperty('simpleResult') ||
          routine.result.hasOwnProperty('powerResult'));

      if (routine.result.hasOwnProperty('simpleResult')) {
        controller.setFakeStandardRoutineResult(
            /** @type {!RoutineType} */ (routine.type),
            /** @type {!StandardRoutineResult} */
            (routine.result.simpleResult));
      } else {
        assertTrue(routine.result.powerResult.hasOwnProperty('simpleResult'));
        controller.setFakePowerRoutineResult(
            /** @type {!RoutineType} */ (routine.type),
            /** @type {!PowerRoutineResult} */
            (routine.result.powerResult));
      }

      // Build the list of routines to run.
      routineTypes.push(routine.type);

      // Add the "running" callback to the list.
      let status =
          new ResultStatusItem(routine.type, ExecutionProgress.kRunning);
      expectedCallbacks.push(status);

      // Add the "completed" callback to the list.
      status = new ResultStatusItem(routine.type, ExecutionProgress.kCompleted);
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
        if (expectedCallbacks[upto].result.hasOwnProperty('simpleResult')) {
          assertEquals(
              expectedCallbacks[upto].result.simpleResult,
              status.result.simpleResult);
        } else {
          assertEquals(
              expectedCallbacks[upto].result.powerResult.simpleResult,
              status.result.powerResult.simpleResult);
        }
      }

      upto++;
    };

    return executor.runRoutines(routineTypes, statusCallback).then(() => {
      // Ensure that all the callbacks were sent.
      assertEquals(expectedCallbacks.length, upto);
    });
  }

  test('SingleTest', () => {
    /** @type {!Array<!RoutineResultInfo>} */
    const routines = [{
      type: chromeos.diagnostics.mojom.RoutineType.kCpuStress,
      result: {
        simpleResult:
            chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed
      }
    }];
    return runRoutinesAndAssertResults(routines);
  });

  test('MultipleTests', () => {
    /** @type {!Array<!RoutineResultInfo>} */
    const routines = [
      {
        type: chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        result: {
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed
        }
      },
      {
        type: chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        result: {
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed
        }
      },
      {
        type: chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
        result: {
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed
        }
      },
      {
        type: chromeos.diagnostics.mojom.RoutineType.kCpuPrime,
        result: {
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed
        }
      },
      {
        type: chromeos.diagnostics.mojom.RoutineType.kBatteryCharge,
        result: {
          powerResult: {
            simpleResult:
                chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed,
            isCharging: true,
            percentDelta: 10,
            timeDeltaSeconds: 10
          }
        }
      },
    ];

    return runRoutinesAndAssertResults(routines);
  });
}
