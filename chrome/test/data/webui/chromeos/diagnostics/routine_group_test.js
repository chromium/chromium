// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineResult, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {createRoutine} from 'chrome://diagnostics/diagnostics_utils.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

/**
 * @param {!RoutineType} routineType
 * @return {!ResultStatusItem}
 */
function getRoutineRunningStatusItem(routineType) {
  return new ResultStatusItem(routineType, ExecutionProgress.kRunning);
}

/**
 * @param {!RoutineType} routineType
 * @return {!ResultStatusItem}
 */
function getRoutinedPassedStatusItem(routineType) {
  let item = new ResultStatusItem(routineType, ExecutionProgress.kCompleted);
  item.result = /** @type {!RoutineResult} */ (
      {simpleResult: StandardRoutineResult.kTestPassed});
  return item;
}

/**
 * @param {!RoutineType} routineType
 * @return {!ResultStatusItem}
 */
function getRoutinedFailedStatusItem(routineType) {
  let item = new ResultStatusItem(routineType, ExecutionProgress.kCompleted);
  item.result = /** @type {!RoutineResult} */ (
      {simpleResult: StandardRoutineResult.kTestFailed});
  return item;
}

/**
 * Get nonBlockingRoutines_ private member for testing.
 * @suppress {visibility} // access private member
 * @param {!RoutineGroup} routineGroup
 * @return {!Set<!RoutineType>}
 */
function getNonBlockingRoutines(routineGroup) {
  return routineGroup.nonBlockingRoutines_;
}

export function routineGroupTestSuite() {
  const {kSignalStrength, kHasSecureWiFiConnection, kCaptivePortal} =
      RoutineType;
  test('GroupStatusSetCorrectly', () => {
    let routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
        ],
        'wifiGroupText');

    let signalStrengthRunning = getRoutineRunningStatusItem(kSignalStrength);
    let signalStrengthCompleted = getRoutinedPassedStatusItem(kSignalStrength);

    // Progress is initially "Not started".
    assertEquals(routineGroup.progress, ExecutionProgress.kNotStarted);

    // Progress should now be running since the signal strength test is in
    // progress.
    routineGroup.setStatus(signalStrengthRunning);
    assertEquals(routineGroup.progress, ExecutionProgress.kRunning);

    // Progress should still be running despite the signal strength test
    // finishing since their are still unfinished routines in this group.
    routineGroup.setStatus(signalStrengthCompleted);
    assertEquals(routineGroup.progress, ExecutionProgress.kRunning);

    let hasSecureWiFiConnectionRunning =
        getRoutineRunningStatusItem(kHasSecureWiFiConnection);
    let hasSecureWiFiConnectionCompleted =
        getRoutinedPassedStatusItem(kHasSecureWiFiConnection);

    // Progress should still be running.
    routineGroup.setStatus(hasSecureWiFiConnectionRunning);
    assertEquals(routineGroup.progress, ExecutionProgress.kRunning);

    // Status should be completed now that all routines in this group
    // have finished running.
    routineGroup.setStatus(hasSecureWiFiConnectionCompleted);
    assertEquals(routineGroup.progress, ExecutionProgress.kCompleted);
  });

  test('TestFailureHandledCorrectly', () => {
    let routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
        ],
        'wifiGroupText');

    let signalStrengthFailed = getRoutinedFailedStatusItem(kSignalStrength);

    routineGroup.setStatus(signalStrengthFailed);
    assertEquals(routineGroup.failedTest, kSignalStrength);
    assertTrue(routineGroup.inWarningState);

    let hasSecureWiFiConnectionFailed =
        getRoutinedFailedStatusItem(kHasSecureWiFiConnection);
    routineGroup.setStatus(hasSecureWiFiConnectionFailed);

    // Failed test does not get overwritten.
    assertEquals(routineGroup.failedTest, kSignalStrength);
    assertTrue(routineGroup.inWarningState);
  });

  test('NonBlockingRoutinesSetInitializedCorrectly', () => {
    let routineGroup = new RoutineGroup(
        [
          createRoutine(kSignalStrength, false),
          createRoutine(kHasSecureWiFiConnection, false),
          createRoutine(kCaptivePortal, true),
        ],
        'wifiGroupText');
    let nonBlockingRoutines = getNonBlockingRoutines(routineGroup);
    assertTrue(nonBlockingRoutines.has(kSignalStrength));
    assertTrue(nonBlockingRoutines.has(kHasSecureWiFiConnection));
    assertFalse(nonBlockingRoutines.has(kCaptivePortal));
  });
}
