// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {RoutineRunnerRemote, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeSystemRoutineContollerTestSuite', function() {
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
    const resolver = new PromiseResolver();

    const routineRunnerRemote = /** @type {!RoutineRunnerRemote} */ ({
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
      },
    });
    controller.runRoutine(expectedType, routineRunnerRemote);
    return controller.getRunRoutinePromiseForTesting().then(() => {
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
    const resolver = new PromiseResolver();

    // Nothing should be running yet.
    assertFalse(controller.isRoutineInProgressForTesting());

    // Use this to detect if the routine resolved too early.
    let wasRun = false;


    const routineRunnerRemote = /** @type {!RoutineRunnerRemote} */ ({
      onRoutineResult: (resultInfo) => {
        assertTrue(controller.isRoutineInProgressForTesting());
        assertFalse(wasRun);
        assertEquals(expectedType, resultInfo.type);
        assertEquals(expectedResult, resultInfo.result.simpleResult);

        // Mark that the test completed.
        wasRun = true;
        resolver.resolve();
      },
    });

    controller.runRoutine(expectedType, routineRunnerRemote);
    controller.getRunRoutinePromiseForTesting().then(() => {
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
        RoutineType.kCpuStress, StandardRoutineResult.kExecutionError);
  });

  test('ExpectedResultPass', () => {
    const routineType = RoutineType.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedResultFail', () => {
    const routineType = RoutineType.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedPowerResultPass', () => {
    const routineType = RoutineType.kCpuCache;
    const expectedResult = StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedPowerResultFail', () => {
    const routineType = RoutineType.kCpuCache;
    const expectedResult = StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    return runRoutineAndAssertStandardResult(routineType, expectedResult);
  });

  test('ExpectedResultPassManualResolve', () => {
    const routineType = RoutineType.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestPassed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineType, expectedResult);
  });

  test('ExpectedResultFailManualResolve', () => {
    const routineType = RoutineType.kCpuStress;
    const expectedResult = StandardRoutineResult.kTestFailed;
    controller.setFakeStandardRoutineResult(routineType, expectedResult);

    // Tests will only resolve when done manually.
    controller.setDelayTimeInMillisecondsForTesting(-1);

    return runRoutineAndAssertStandardResultManualResolve(
        routineType, expectedResult);
  });

  test('GetSupportedRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const expected = [RoutineType.kCpuStress, RoutineType.kCpuCache];

    controller.setFakeSupportedRoutines(expected);
    return controller.getSupportedRoutines().then((result) => {
      assertDeepEquals(expected, result.routines);
    });
  });
});
