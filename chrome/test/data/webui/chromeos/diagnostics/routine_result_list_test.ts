// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_list.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResultListElement} from 'chrome://diagnostics/routine_result_list.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('routineResultListTestSuite', function() {
  let routineResultListElement: RoutineResultListElement|null = null;

  const simpleResultTestPassed = {
    simpleResult: StandardRoutineResult.kTestPassed,
    powerResult: undefined,
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    routineResultListElement?.remove();
    routineResultListElement = null;
  });

  /**
   * Initializes the routine-result-list and sets the list of routines.
   */
  function initializeRoutineResultList(routines: RoutineType[]): Promise<void> {
    assertFalse(!!routineResultListElement);

    // Add the entry to the DOM.
    routineResultListElement =
        document.createElement(RoutineResultListElement.is);
    assertTrue(!!routineResultListElement);
    document.body.appendChild(routineResultListElement);

    // Initialize the routines.
    assert(routineResultListElement);
    routineResultListElement.initializeTestRun(routines);

    return flushTasks();
  }

  /**
   * Clear the routine-result-list.
   */
  function clearRoutineResultList(): Promise<void> {
    assert(routineResultListElement);
    routineResultListElement.clearRoutines();
    return flushTasks();
  }

  /**
   * Returns an array of the entries in the list.
   */
  function getEntries(): NodeListOf<RoutineResultEntryElement> {
    assert(routineResultListElement);
    return dx_utils.getResultEntries(routineResultListElement);
  }

  test('ElementRendered', async () => {
    await initializeRoutineResultList([]);
    assert(routineResultListElement);
    // Verify the element rendered.
    assertTrue(isVisible(strictQuery(
        '#resultListContainer', routineResultListElement.shadowRoot,
        HTMLDivElement)));
  });

  test('HideElement', async () => {
    await initializeRoutineResultList([]);
    assert(routineResultListElement);
    assertFalse(routineResultListElement.hidden);
    assertFalse(strictQuery(
                    '#resultListContainer', routineResultListElement.shadowRoot,
                    HTMLElement)
                    .hidden);
    routineResultListElement.hidden = true;
    flushTasks();
    assertTrue(routineResultListElement!.hidden);
    assert(routineResultListElement);
    assertTrue(strictQuery(
                   '#resultListContainer', routineResultListElement.shadowRoot,
                   HTMLElement)
                   .hidden);
  });

  test('EmptyByDefault', async () => {
    await initializeRoutineResultList([]);
    assertEquals(0, getEntries().length);
  });

  test('InitializedRoutines', async () => {
    const routines: RoutineType[] = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    await initializeRoutineResultList(routines);
    assertEquals(routines.length, getEntries().length);
    getEntries().forEach((entry: RoutineResultEntryElement, index: number) => {
      // Routines are initialized in the unstarted state.
      const routine = routines[index] as RoutineType;
      const status = new ResultStatusItem(routine);
      assertDeepEquals(status, entry.item);
    });
  });

  test('InitializeThenClearRoutines', async () => {
    const routines: RoutineType[] = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    await initializeRoutineResultList(routines);
    assertEquals(routines.length, getEntries().length);
    await clearRoutineResultList();
    // List is empty after clearing.
    assertEquals(0, getEntries().length);
  });

  test('VerifyStatusUpdates', async () => {
    const routines: RoutineType[] = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      // Verify the starting state.
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach(
          (entry: RoutineResultEntryElement, index: number) => {
            // Routines are initialized in the unstarted state.
            const status = new ResultStatusItem(routines[index] as RoutineType);
            assertDeepEquals(status, entry.item);
          });

      let status = new ResultStatusItem(
          routines[0] as RoutineType, ExecutionProgress.RUNNING);
      assert(routineResultListElement);
      routineResultListElement.onStatusUpdate(status);
      return flushTasks()
          .then(() => {
            // Verify first routine is running.
            const entry = getEntries()[0];
            assert(entry);
            const item = entry.item as ResultStatusItem;
            assertEquals(ExecutionProgress.RUNNING, item.progress);
            assertEquals(null, item.result);

            // Move the first routine to completed state.
            status = new ResultStatusItem(
                routines[0] as RoutineType, ExecutionProgress.COMPLETED);
            status.result = simpleResultTestPassed;
            assert(routineResultListElement);
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the first routine is completed.
            const entry = getEntries()[0];
            assert(entry);
            const item = entry.item as ResultStatusItem;
            assertEquals(ExecutionProgress.COMPLETED, item.progress);
            assertNotEquals(null, item.result);
            assertEquals(
                StandardRoutineResult.kTestPassed, item.result!.simpleResult);

            status = new ResultStatusItem(
                routines[1] as RoutineType, ExecutionProgress.RUNNING);
            assert(routineResultListElement);
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify second routine is running.
            const entry = getEntries()[1];
            assert(entry);
            const item = entry.item as ResultStatusItem;
            assertEquals(ExecutionProgress.RUNNING, item.progress);
            assertEquals(null, item.result);

            // Move the second routine to completed state.
            status = new ResultStatusItem(
                routines[1] as RoutineType, ExecutionProgress.COMPLETED);
            status.result = simpleResultTestPassed;
            assert(routineResultListElement);
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the second routine is completed.
            const entry = getEntries()[1];
            assert(entry);
            const item = entry.item as ResultStatusItem;
            assertEquals(ExecutionProgress.COMPLETED, item.progress);
            assertNotEquals(null, item.result);
            return flushTasks();
          });
    });
  });
});
