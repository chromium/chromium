// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineRunner, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function fakeSystemRoutineContollerTestSuite() {
  /** @type {?FakeSystemRoutineController} */
  let controller = null;

  setup(() => {
    controller = new FakeSystemRoutineController();
  });

  teardown(() => {
    controller = null;
  });

  /**
   * Runs a standard routine and asserts the expected result. The returned
   * promise must be forced to resolve either by chaining it with additional
   * promises or returning it from the test.
   *
   * @param {!RoutineType} expectedType
   * @param {!StandardRoutineResult} expectedResult
   * @return {!Promise}
   */
  function runRoutineAndAssertStandardResult(expectedType, expectedResult) {
    let resolver = new PromiseResolver();

    const routineRunnerRemote = /** @type {!RoutineRunner} */ ({
      onRoutineResult: (resultInfo) => {
        assertEquals(expectedType, resultInfo.type);

        if (resultInfo.result.hasOwnProperty('simpleResult')) {
          assertEquals(expectedResult, resultInfo.result.simpleResult);

          // Can't have both simpleResult and powerResult
          assertFalse(resultInfo.result.hasOwnProperty('powerResult'));
        }

        if (resultInfo.result.hasOwnProperty('powerResult')) {
          assertEquals(expectedResult, resultInfo.result.powerResult.result);

          // Can't have both simpleResult and powerResult
          assertFalse(resultInfo.result.hasOwnProperty('simpleResult'));
        }

        resolver.resolve();
      }
    });

    return controller.runRoutine(expectedType, routineRunnerRemote).then(() => {
      return resolver.promise;
    });
  }

  /**
   * Runs a standard routine and asserts the expected result. The returned
   * promise must be forced to resolve either by chaining it with additional
   * promises or returning it from the test.
   *
   * @param {!RoutineType} expectedType
   * @param {!StandardRoutineResult} expectedResult
   * @return {!Promise}
   */
  function runRoutineAndAssertStandardResultManualResolve(
      expectedType, expectedResult) {
    let resolver = new PromiseResolver();

    // Nothing should be running yet.
    assertFalse(controller.isRoutineInProgressForTesting());

    // Use this to detect if the routine resolved too early.
    let wasRun = false;


    const routineRunnerRemote = /** @type {!RoutineRunner} */ ({
      onRoutineResult: (resultInfo) => {
        assertTrue(controller.isRoutineInProgressForTesting());
        assertFalse(wasRun);
        assertEquals(expectedType, resultInfo.type);
        assertEquals(expectedResult, resultInfo.result.simpleResult);

        // Mark that the test completed.
        wasRun = true;
        resolver.resolve();
      }
    });

    controller.runRoutine(expectedType, routineRunnerRemote).then(() => {
      assertTrue(wasRun);
      assertFalse(controller.isRoutineInProgressForTesting());
    });

    // Set a short delay and verify that the routine is still running.
    setTimeout(() => {
      assertFalse(wasRun);
      assertTrue(controller.isRoutineInProgressForTesting());

      // Manually resolve the test.
      controller.resolveRoutineForTesting().then(() => {
        assertTrue(wasRun);
      });
    }, 5);

    return resolver.promise;
  }

  test('NonExistantTest', () => {
    // A routine that hasn't had a fake result set will return kErrorExecuting.
    return runRoutineAndAssertStandardResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        chromeos.diagnostics.mojom.StandardRoutineResult.kExecutionError);
  });

  test('ExpectedResultPass', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuStress;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedResultFail', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuStress;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedPowerResultPass', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuCache;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedPowerResultFail', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuCache;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedResultPassManualResolve', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuStress;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineType, expectedResult);
  });

  test('ExpectedResultFailManualResolve', () => {
    const routineType = chromeos.diagnostics.mojom.RoutineType.kCpuStress;
    const expectedResult =
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineType, expectedResult);
  });

  test('GetSupportedRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const expected = [
      chromeos.diagnostics.mojom.RoutineType.kCpuStress,
      chromeos.diagnostics.mojom.RoutineType.kCpuCache
    ];

    controller.setFakeSupportedRoutines(expected);
    return controller.getSupportedRoutines().then((result) => {
      assertDeepEquals(expected, result.routines);
    });
  });
}
