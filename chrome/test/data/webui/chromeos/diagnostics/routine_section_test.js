// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_result_entry.js';
import 'chrome://diagnostics/routine_section.js';

import {RoutineName} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress} from 'chrome://diagnostics/routine_list_executor.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

import * as diagnostics_test_utils from './diagnostics_test_utils.js';

suite('RoutineSectionTest', () => {
  /** @type {?HTMLElement} */
  let routineSectionElement = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (routineSectionElement) {
      routineSectionElement.remove();
    }
    routineSectionElement = null;
  });

  /**
   * Initializes the element and sets the routines.
   * @param {!Array<!RoutineName>} routines
   */
  function initializeRoutineSection(routines) {
    assertFalse(!!routineSectionElement);

    // Add the entry to the DOM.
    routineSectionElement = document.createElement('routine-section');
    assertTrue(!!routineSectionElement);
    document.body.appendChild(routineSectionElement);

    // Assign the routines to the property.
    routineSectionElement.routines = routines;

    return flushTasks();
  }

  /**
   * Returns the result list element.
   * @return {!RoutineList}
   */
  function getResultList() {
    const resultList =
        diagnostics_test_utils.getResultList(routineSectionElement);
    assertTrue(!!resultList);
    return resultList;
  }

  /**
   * Returns the Run Tests button.
   * @return {!CrButton}
   */
  function getRunTestsButton() {
    const button = routineSectionElement.$$('#runTestsButton');
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {bool}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
  }

  /**
   * Clicks the run tests button.
   * @return {!Promise}
   */
  function clickRunTestsButton() {
    getRunTestsButton().click();
    return flushTasks();
  }

  /**
   * Returns an array of the entries in the list.
   * @return {!Array<!RoutineResultEntry>}
   */
  function getEntries() {
    return diagnostics_test_utils.getResultEntries(getResultList());
  }

  test('ElementRenders', () => {
    return initializeRoutineSection([]).then(() => {
      // Verify the element rendered.
      assertTrue(!!routineSectionElement.$$('#routineSection'));
    });
  });

  test('ClickButtonDisablesButton', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          assertFalse(isRunTestsButtonDisabled());
          return clickRunTestsButton();
        })
        .then(() => {
          assertTrue(isRunTestsButtonDisabled());
        });
  });

  test('ClickButtonInitializesResultList', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // No result entries initially.
          assertEquals(0, getEntries().length);
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);
          entries.forEach((entry, index) => {
            assertEquals(routines[index], entry.item.routine);
            assertEquals(ExecutionProgress.kNotStarted, entry.item.progress);
          });
        });
  });
});
