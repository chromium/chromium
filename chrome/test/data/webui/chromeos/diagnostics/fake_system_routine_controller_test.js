// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineName, RoutineRunner, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

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
   * @param {!RoutineName} expectedName
   * @param {!StandardRoutineResult} expectedResult
   * @return {!Promise}
   */
  function runRoutineAndAssertStandardResult(expectedName, expectedResult) {
    let resolver = new PromiseResolver();

    /** @type {!RoutineRunner} */
    const routineRunnerRemote = {
      onRoutineResult: (resultInfo) => {
        assertEquals(expectedName, resultInfo.name);
        assertEquals(expectedResult, resultInfo.result.simpleResult);
        resolver.resolve();
      }
    };

    return controller.runRoutine(expectedName, routineRunnerRemote).then(() => {
      return resolver.promise;
    });
  }

  /**
   * Runs a standard routine and asserts the expected result. The returned
   * promise must be forced to resolve either by chaining it with additional
   * promises or returning it from the test.
   *
   * @param {!RoutineName} expectedName
   * @param {!StandardRoutineResult} expectedResult
   * @return {!Promise}
   */
  function runRoutineAndAssertStandardResultManualResolve(
      expectedName, expectedResult) {
    let resolver = new PromiseResolver();

    // Nothing should be running yet.
    assertFalse(controller.isRoutineInProgressForTesting());

    // Use this to detect if the routine resolved too early.
    let wasRun = false;

    /** @type {!RoutineRunner} */
    const routineRunnerRemote = {
      onRoutineResult: (resultInfo) => {
        assertTrue(controller.isRoutineInProgressForTesting());
        assertFalse(wasRun);
        assertEquals(expectedName, resultInfo.name);
        assertEquals(expectedResult, resultInfo.result.simpleResult);

        // Mark that the test completed.
        wasRun = true;
        resolver.resolve();
      }
    };

    controller.runRoutine(expectedName, routineRunnerRemote).then(() => {
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
        RoutineName.kCpuStress, StandardRoutineResult.kErrorExecuting);
  });

  test('ExpectedResultPass', () => {
    const routineName = RoutineName.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineName, expectedResult);

    return runRoutineAndAssertStandardResult(routineName, expectedResult);
  });

  test('ExpectedResultFail', () => {
    const routineName = RoutineName.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineName, expectedResult);

    return runRoutineAndAssertStandardResult(routineName, expectedResult);
  });

  test('ExpectedResultPassManualResolve', () => {
    const routineName = RoutineName.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineName, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineName, expectedResult);
  });

  test('ExpectedResultFailManualResolve', () => {
    const routineName = RoutineName.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineName, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineName, expectedResult);
  });
}
