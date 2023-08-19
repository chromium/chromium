// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_list.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResultListElement} from 'chrome://diagnostics/routine_result_list.js';
import {RoutineResult, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('routineResultListTestSuite', function() {
  /** @type {?RoutineResultListElement} */
  let routineResultListElement = null;

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(function() {
    if (routineResultListElement) {
      routineResultListElement.remove();
    }
    routineResultListElement = null;
  });

  /**
   * Initializes the routine-result-list and sets the list of routines.
   * @param {!Array<!RoutineType>} routines
   * @return {!Promise}
   */
  function initializeRoutineResultList(routines) {
    assertFalse(!!routineResultListElement);

    // Add the entry to the DOM.
    routineResultListElement = /** @type {!RoutineResultListElement} */ (
        document.createElement('routine-result-list'));
    assertTrue(!!routineResultListElement);
    document.body.appendChild(routineResultListElement);

    // Initialize the routines.
    routineResultListElement.initializeTestRun(routines);

    return flushTasks();
  }

  /**
   * Clear the routine-result-list.
   * @return {!Promise}
   */
  function clearRoutineResultList() {
    routineResultListElement.clearRoutines();
    return flushTasks();
  }

  /**
   * Returns an array of the entries in the list.
   * @return {!NodeList<!RoutineResultEntryElement>}
   */
  function getEntries() {
    return dx_utils.getResultEntries(routineResultListElement);
  }

  test('ElementRendered', () => {
    return initializeRoutineResultList([]).then(() => {
      // Verify the element rendered.
      const div = routineResultListElement.shadowRoot.querySelector(
          '#resultListContainer');
      assertTrue(!!div);
    });
  });

  test('HideElement', () => {
    return initializeRoutineResultList([])
        .then(() => {
          assertFalse(routineResultListElement.hidden);
          assertFalse(routineResultListElement.shadowRoot
                          .querySelector('#resultListContainer')
                          .hidden);
          routineResultListElement.hidden = true;
          return flushTasks();
        })
        .then(() => {
          assertTrue(routineResultListElement.hidden);
          assertTrue(routineResultListElement.shadowRoot
                         .querySelector('#resultListContainer')
                         .hidden);
        });
  });

  test('EmptyByDefault', () => {
    return initializeRoutineResultList([]).then(() => {
      assertEquals(0, getEntries().length);
    });
  });

  test('InitializedRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach((entry, index) => {
        // Routines are initialized in the unstarted state.
        const status = new ResultStatusItem(routines[index]);
        assertDeepEquals(status, entry.item);
      });
    });
  });

  test('InitializeThenClearRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineResultList(routines)
        .then(() => {
          assertEquals(routines.length, getEntries().length);
          return clearRoutineResultList();
        })
        .then(() => {
          // List is empty after clearing.
          assertEquals(0, getEntries().length);
        });
  });

  test('VerifyStatusUpdates', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      // Verify the starting state.
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach((entry, index) => {
        // Routines are initialized in the unstarted state.
        const status = new ResultStatusItem(routines[index]);
        assertDeepEquals(status, entry.item);
      });

      let status = new ResultStatusItem(routines[0], ExecutionProgress.RUNNING);
      routineResultListElement.onStatusUpdate(status);
      return flushTasks()
          .then(() => {
            // Verify first routine is running.
            assertEquals(
                ExecutionProgress.RUNNING, getEntries()[0].item.progress);
            assertEquals(null, getEntries()[0].item.result);

            // Move the first routine to completed state.
            status =
                new ResultStatusItem(routines[0], ExecutionProgress.COMPLETED);
            status.result = /** @type {!RoutineResult} */ (
                {simpleResult: StandardRoutineResult.kTestPassed});
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the first routine is completed.
            assertEquals(
                ExecutionProgress.COMPLETED, getEntries()[0].item.progress);
            assertNotEquals(null, getEntries()[0].item.result);
            assertEquals(
                StandardRoutineResult.kTestPassed,
                getEntries()[0].item.result.simpleResult);

            status =
                new ResultStatusItem(routines[1], ExecutionProgress.RUNNING);
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify second routine is running.
            assertEquals(
                ExecutionProgress.RUNNING, getEntries()[1].item.progress);
            assertEquals(null, getEntries()[1].item.result);

            // Move the second routine to completed state.
            status =
                new ResultStatusItem(routines[1], ExecutionProgress.COMPLETED);
            status.result = /** @type {!RoutineResult} */ (
                {simpleResult: StandardRoutineResult.kTestPassed});
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the second routine is completed.
            assertEquals(
                ExecutionProgress.COMPLETED, getEntries()[1].item.progress);
            assertNotEquals(null, getEntries()[1].item.result);
            assertEquals(
                StandardRoutineResult.kTestPassed,
                getEntries()[0].item.result.simpleResult);

            return flushTasks();
          });
    });
  });
});
