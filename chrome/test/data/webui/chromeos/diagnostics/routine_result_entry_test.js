// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_result_entry.js';

import {RoutineName, RoutineResult, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('RoutineResultEntryTest', () => {
  /** @type {?HTMLElement} */
  let routineResultEntryElement = null;

  setup(function() {
    PolymerTest.clearBody();
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
    routineResultEntryElement = document.createElement('routine-result-entry');
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
        {simple_result: StandardRoutineResult.kTestPassed});
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
        {simple_result: StandardRoutineResult.kTestFailed});
    return initializeEntryWithItem(item).then(() => {
      // TODO(zentaro): Localize the test.
      assertEquals(getNameText(), 'kCpuStress');

      // Status should show the passed result.
      assertEquals(getStatusText(), 'kTestFailed');
    });
  });
});
