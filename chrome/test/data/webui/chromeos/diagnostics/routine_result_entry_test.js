// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_entry.js';

import {RoutineName, RoutineResult, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

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
   * Creates a result status item without a final result.
   * @param {!RoutineName} routine
   * @param {!ExecutionProgress} progress
   * @return {!ResultStatusItem}
   */
  function createIncompleteStatus(routine, progress) {
    let status = new ResultStatusItem(routine);
    status.progress = progress;
    return status;
  }

  /**
   * Creates a completed result status item with a result.
   * @param {!RoutineName} routine
   * @param {!RoutineResult} result
   * @return {!ResultStatusItem}
   */
  function createCompletedStatus(routine, result) {
    let status = createIncompleteStatus(routine, ExecutionProgress.kCompleted);
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
   * Returns the status element text content.
   * @return {string}
   */
  function getStatusText() {
    const status = routineResultEntryElement.$$('#status');
    assertTrue(!!status);
    return status.textContent.trim();
  }

  test('ElementRendered', () => {
    return initializeRoutineResultEntry().then(() => {
      // Verify the element rendered.
      let div = routineResultEntryElement.$$('.entryRow');
      assertTrue(!!div);
    });
  });

  test('NotStartedTest', () => {
    const item = createIncompleteStatus(
        RoutineName.kCpuStress, ExecutionProgress.kNotStarted);
    return initializeEntryWithItem(item).then(() => {
      // TODO(zentaro): Localize the test.
      assertEquals(getNameText(), 'kCpuStress');

      // Status should be empty if the test is not started.
      assertEquals(getStatusText(), '');
    });
  });

  test('RunningTest', () => {
    const item = createIncompleteStatus(
        RoutineName.kCpuStress, ExecutionProgress.kRunning);
    return initializeEntryWithItem(item).then(() => {
      // TODO(zentaro): Localize the test.
      assertEquals(getNameText(), 'kCpuStress');

      // Status should be running.
      assertEquals(getStatusText(), 'kRunning');
    });
  });

  test('PassedTest', () => {
    const item = createCompletedStatus(
        RoutineName.kCpuStress,
        {simpleResult: StandardRoutineResult.kTestPassed});
    return initializeEntryWithItem(item).then(() => {
      // TODO(zentaro): Localize the test.
      assertEquals(getNameText(), 'kCpuStress');

      // Status should show the passed result.
      assertEquals(getStatusText(), 'kTestPassed');
    });
  });

  test('FailedTest', () => {
    const item = createCompletedStatus(
        RoutineName.kCpuStress,
        {simpleResult: StandardRoutineResult.kTestFailed});
    return initializeEntryWithItem(item).then(() => {
      // TODO(zentaro): Localize the test.
      assertEquals(getNameText(), 'kCpuStress');

      // Status should show the passed result.
      assertEquals(getStatusText(), 'kTestFailed');
    });
  });
}
