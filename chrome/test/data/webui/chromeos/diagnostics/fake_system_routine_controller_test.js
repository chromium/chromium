// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {RoutineName, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

suite('FakeSystemRoutineContollerTest', () => {
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
        assertEquals(expectedResult, resultInfo.result.simple_result);
        resolver.resolve();
      }
    };

    return controller.runRoutine(expectedName, routineRunnerRemote).then(() => {
      return resolver.promise;
    });
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
});
