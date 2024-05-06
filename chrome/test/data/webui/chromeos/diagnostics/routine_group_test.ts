// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {createRoutine} from 'chrome://diagnostics/diagnostics_utils.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

function getRoutineRunningStatusItem(routineType: RoutineType):
    ResultStatusItem {
  return new ResultStatusItem(routineType, ExecutionProgress.RUNNING);
}

function getRoutinedPassedStatusItem(routineType: RoutineType):
    ResultStatusItem {
  const item = new ResultStatusItem(routineType, ExecutionProgress.COMPLETED);
  item.result = {
    simpleResult: StandardRoutineResult.kTestPassed,
    powerResult: undefined,
  };
  return item;
}

function getRoutinedFailedStatusItem(routineType: RoutineType):
    ResultStatusItem {
  const item = new ResultStatusItem(routineType, ExecutionProgress.COMPLETED);
  item.result = {
    simpleResult: StandardRoutineResult.kTestFailed,
    powerResult: undefined,
  };
  return item;
}

/**
 * Get nonBlockingRoutines private member for testing.
 */
function getNonBlockingRoutines(routineGroup: RoutineGroup): Set<RoutineType> {
  return routineGroup.nonBlockingRoutines;
}

suite('routineGroupTestSuite', function() {
  const {kSignalStrength, kHasSecureWiFiConnection, kCaptivePortal} =
      RoutineType;
  test('GroupStatusSetCorrectly', () => {
    const routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
        ],
        'wifiGroupText');

    const signalStrengthRunning = getRoutineRunningStatusItem(kSignalStrength);
    const signalStrengthCompleted =
        getRoutinedPassedStatusItem(kSignalStrength);

    // Progress is initially "Not started".
    assertEquals(routineGroup.progress, ExecutionProgress.NOT_STARTED);

    // Progress should now be running since the signal strength test is in
    // progress.
    routineGroup.setStatus(signalStrengthRunning);
    assertEquals(routineGroup.progress, ExecutionProgress.RUNNING);

    // Progress should still be running despite the signal strength test
    // finishing since their are still unfinished routines in this group.
    routineGroup.setStatus(signalStrengthCompleted);
    assertEquals(routineGroup.progress, ExecutionProgress.RUNNING);

    const hasSecureWiFiConnectionRunning =
        getRoutineRunningStatusItem(kHasSecureWiFiConnection);
    const hasSecureWiFiConnectionCompleted =
        getRoutinedPassedStatusItem(kHasSecureWiFiConnection);

    // Progress should still be running.
    routineGroup.setStatus(hasSecureWiFiConnectionRunning);
    assertEquals(routineGroup.progress, ExecutionProgress.RUNNING);

    // Status should be completed now that all routines in this group
    // have finished running.
    routineGroup.setStatus(hasSecureWiFiConnectionCompleted);
    assertEquals(routineGroup.progress, ExecutionProgress.COMPLETED);
  });

  test('TestFailureHandledCorrectly', () => {
    const routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
        ],
        'wifiGroupText');

    const signalStrengthFailed = getRoutinedFailedStatusItem(kSignalStrength);

    routineGroup.setStatus(signalStrengthFailed);
    assertEquals(routineGroup.failedTest, kSignalStrength);
    assertTrue(routineGroup.inWarningState);

    const hasSecureWiFiConnectionFailed =
        getRoutinedFailedStatusItem(kHasSecureWiFiConnection);
    routineGroup.setStatus(hasSecureWiFiConnectionFailed);

    // Failed test does not get overwritten.
    assertEquals(routineGroup.failedTest, kSignalStrength);
    assertTrue(routineGroup.inWarningState);
  });

  test('NonBlockingRoutinesSetInitializedCorrectly', () => {
    const routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
          createRoutine(kCaptivePortal, true),
        ],
        'wifiGroupText');
    const nonBlockingRoutines = getNonBlockingRoutines(routineGroup);
    assertTrue(nonBlockingRoutines.has(kSignalStrength));
    assertTrue(nonBlockingRoutines.has(kHasSecureWiFiConnection));
    assertFalse(nonBlockingRoutines.has(kCaptivePortal));
  });
});
