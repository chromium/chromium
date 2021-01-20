// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_entry.js';

import {RoutineResult, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {BadgeType} from 'chrome://diagnostics/text_badge.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function routineResultEntryTestSuite() {
  /** @type {?RoutineResultEntryElement} */
  let routineResultEntryElement = null;

  setup(function() {
    document.body.innerHTML = '';
  });

  teardown(function() {
    if (routineResultEntryElement) {
      routineResultEntryElement.remove();
    }
    routineResultEntryElement = null;
  });

  function initializeRoutineResultEntry() {
    assertFalse(!!routineResultEntryElement);

    // Add the entry to the DOM.
    routineResultEntryElement = /** @type {!RoutineResultEntryElement} */ (
        document.createElement('routine-result-entry'));
    assertTrue(!!routineResultEntryElement);
    document.body.appendChild(routineResultEntryElement);

    return flushTasks();
  }

  /**
   * Updates the item in the element.
   * @param {!ResultStatusItem} item
   * @return {!Promise}
   */
  function updateItem(item) {
    routineResultEntryElement.item = item;
    return flushTasks();
  }

  /**
   * Initializes the entry then updates the item.
   * @param {!ResultStatusItem} item
   * @return {!Promise}
   */
  function initializeEntryWithItem(item) {
    return initializeRoutineResultEntry().then(() => {
      return updateItem(item);
    });
  }

  /**
   * Creates a completed result status item with a result.
   * @param {!RoutineType} routine
   * @param {!RoutineResult} result
   * @return {!ResultStatusItem}
   */
  function createCompletedStatus(routine, result) {
    let status = new ResultStatusItem(routine, ExecutionProgress.kCompleted);
    status.result = result;
    return status;
  }

  /**
   * Returns the routine name element text content.
   * @return {string}
   */
  function getNameText() {
    const name = routineResultEntryElement.$$('#routine');
    assertTrue(!!name);
    return name.textContent.trim();
  }

  /**
   * Returns the status badge content.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    const badge = /** @type{!TextBadgeElement} */ (
        routineResultEntryElement.$$('#status'));
    assertTrue(!!badge);
    return badge;
  }

  test('ElementRendered', () => {
    return initializeRoutineResultEntry().then(() => {
      // Verify the element rendered.
      let div = routineResultEntryElement.$$('.entryRow');
      assertTrue(!!div);
    });
  });

  test('NotStartedTest', () => {
    const item =
        new ResultStatusItem(chromeos.diagnostics.mojom.RoutineType.kCpuStress);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should be queued if the test is not started.
      assertTrue(isVisible(getStatusBadge()));
      assertEquals(getStatusBadge().badgeType, BadgeType.QUEUED);
      dx_utils.assertTextContains(
          getStatusBadge().value,
          loadTimeData.getString('testQueuedBadgeText'));
    });
  });

  test('RunningTest', () => {
    const item = new ResultStatusItem(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        ExecutionProgress.kRunning);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should be running.
      dx_utils.assertTextContains(
          getStatusBadge().value,
          loadTimeData.getString('testRunningBadgeText'));
      assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
    });
  });

  test('PassedTest', () => {
    const item = createCompletedStatus(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        /** @type {!RoutineResult} */ ({
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'SUCCESS');
      assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
    });
  });

  test('FailedTest', () => {
    const item = createCompletedStatus(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        /** @type {!RoutineResult} */ ({
          simpleResult:
              chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'FAILED');
      assertEquals(getStatusBadge().badgeType, BadgeType.ERROR);
    });
  });

  test('StoppedTest', () => {
    const item = new ResultStatusItem(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        ExecutionProgress.kCancelled);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show that the test was stopped.
      assertEquals(
          getStatusBadge().value,
          loadTimeData.getString('testStoppedBadgeText'));
      assertEquals(getStatusBadge().badgeType, BadgeType.STOPPED);
    });
  });

  test('PowerTest', () => {
    const item = createCompletedStatus(
        chromeos.diagnostics.mojom.RoutineType.kBatteryCharge,
        /** @type {!RoutineResult} */ ({
          powerResult: {
            simpleResult:
                chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed,
            isCharging: true,
            percentDelta: 10,
            timeDeltaSeconds: 10
          }
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('batteryChargeRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'SUCCESS');
      assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
    });
  });
}
