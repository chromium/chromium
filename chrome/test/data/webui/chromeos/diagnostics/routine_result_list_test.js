// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_result_list.js';

import {RoutineName} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('RoutineResultListTest', () => {
  /** @type {?HTMLElement} */
  let routineResultListElement = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (routineResultListElement) {
      routineResultListElement.remove();
    }
    routineResultListElement = null;
  });

  /**
   * Initializes the routine-result-list and sets the list of routines.
   * @param {!Array<!RoutineName>} routines
   * @return {!Promise}
   */
  function initializeRoutineResultList(routines) {
    assertFalse(!!routineResultListElement);

    // Add the entry to the DOM.
    routineResultListElement = document.createElement('routine-result-list');
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
   * @return {!Array<!RoutineResultEntry>}
   */
  function getEntries() {
    return dx_utils.getResultEntries(routineResultListElement);
  }

  test('ElementRendered', () => {
    return initializeRoutineResultList([]).then(() => {
      // Verify the element rendered.
      let div = routineResultListElement.$$('#resultListContainer');
      assertTrue(!!div);
    });
  });

  test('EmptyByDefault', () => {
    return initializeRoutineResultList([]).then(() => {
      assertEquals(0, getEntries().length);
    });
  });

  test('InitializedRoutines', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach((entry, index) => {
        // Routines are initialized in the unstarted state.
        let status = new ResultStatusItem(routines[index]);
        status.progress = ExecutionProgress.kNotStarted;
        assertDeepEquals(status, entry.item);
      });
    });
  });

  test('InitializeThenClearRoutines', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
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
});
