// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {ExecutionProgress, ResultStatusItem, RoutineListExecutor} from 'chrome://diagnostics/routine_list_executor.js';
import {PowerRoutineResult, RoutineResultInfo, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeRoutineListExecutorTestSuite', function() {
  let controller: FakeSystemRoutineController|null = null;

  let executor: RoutineListExecutor|null = null;

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
   */
  function runRoutinesAndAssertResults(routines: RoutineResultInfo[]):
      Promise<void> {
    const expectedCallbacks: ResultStatusItem[] = [];
    const routineTypes: RoutineType[] = [];
    routines.forEach((routine: RoutineResultInfo) => {
      assert(controller);
      // Set the result into the fake.
      assertNotEquals(undefined, routine);
      assertNotEquals(undefined, routine.result);

      // simpleResult or powerResult must exist.
      assertTrue(
          routine.result.hasOwnProperty('simpleResult') ||
          routine.result.hasOwnProperty('powerResult'));

      if (routine.result.hasOwnProperty('simpleResult')) {
        controller.setFakeStandardRoutineResult(
            routine.type,
            routine.result!.simpleResult as StandardRoutineResult);
      } else {
        assertTrue(routine.result!.powerResult!.hasOwnProperty('simpleResult'));
        controller.setFakePowerRoutineResult(
            routine.type, routine.result!.powerResult as PowerRoutineResult);
      }

      // Build the list of routines to run.
      routineTypes.push(routine.type);

      // Add the "running" callback to the list.
      let status =
          new ResultStatusItem(routine.type, ExecutionProgress.RUNNING);
      expectedCallbacks.push(status);

      // Add the "completed" callback to the list.
      status = new ResultStatusItem(routine.type, ExecutionProgress.COMPLETED);
      status.result = routine.result;
      expectedCallbacks.push(status);
    });

    // Create the callback that validates the arguments.
    let callbackIndex = 0;

    const statusCallback = (status: ResultStatusItem) => {
      assertTrue(callbackIndex < expectedCallbacks.length);
      assertEquals(expectedCallbacks[callbackIndex]!.routine, status.routine);
      assertEquals(expectedCallbacks[callbackIndex]!.progress, status.progress);
      if (status.progress === ExecutionProgress.RUNNING) {
        assertEquals(null, status.result);
      } else {
        if (expectedCallbacks[callbackIndex]!.result!.hasOwnProperty(
                'simpleResult')) {
          assertEquals(
              expectedCallbacks[callbackIndex]!.result!.simpleResult,
              status.result!.simpleResult);
        } else {
          assertEquals(
              expectedCallbacks[callbackIndex]!.result!.powerResult!
                  .simpleResult,
              status!.result!.powerResult!.simpleResult);
        }
      }

      ++callbackIndex;
    };
    assert(executor);
    return executor.runRoutines(routineTypes, statusCallback).then(() => {
      // Ensure that all the callbacks were sent.
      assertEquals(expectedCallbacks.length, callbackIndex);
    });
  }

  test('SingleTest', () => {
    const routines: RoutineResultInfo[] = [{
      type: RoutineType.kCpuStress,
      result: {
        simpleResult: StandardRoutineResult.kTestFailed,
        powerResult: undefined,
      },
    }];
    return runRoutinesAndAssertResults(routines);
  });

  test('MultipleTests', () => {
    const routines: RoutineResultInfo[] = [
      {
        type: RoutineType.kCpuStress,
        result: {
          simpleResult: StandardRoutineResult.kTestPassed,
          powerResult: undefined,
        },
      },
      {
        type: RoutineType.kCpuCache,
        result: {
          simpleResult: StandardRoutineResult.kTestFailed,
          powerResult: undefined,
        },
      },
      {
        type: RoutineType.kCpuFloatingPoint,
        result: {
          simpleResult: StandardRoutineResult.kTestPassed,
          powerResult: undefined,
        },
      },
      {
        type: RoutineType.kCpuPrime,
        result: {
          simpleResult: StandardRoutineResult.kTestFailed,
          powerResult: undefined,
        },
      },
    ];

    return runRoutinesAndAssertResults(routines);
  });
});
