// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_entry.js';
import 'chrome://diagnostics/routine_section.js';

import {RoutineName, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryRoutineResults, fakeRoutineResults} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {ExecutionProgress} from 'chrome://diagnostics/routine_list_executor.js';
import {BadgeType} from 'chrome://diagnostics/text_badge.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function routineSectionTestSuite() {
  /** @type {?RoutineSectionElement} */
  let routineSectionElement = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  setup(function() {
    document.body.innerHTML = '';

    // Setup a fake routine controller so that nothing resolves unless
    // done explicitly.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakeBatteryRoutineResults.keys()]);

    setSystemRoutineControllerForTesting(routineController);
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
    routineSectionElement = /** @type {!RoutineSectionElement} */ (
        document.createElement('routine-section'));
    assertTrue(!!routineSectionElement);
    document.body.appendChild(routineSectionElement);

    // Assign the routines to the property.
    routineSectionElement.routines = routines;

    return flushTasks();
  }

  /**
   * Returns the result list element.
   * @return {!RoutineResultListElement}
   */
  function getResultList() {
    const resultList = dx_utils.getResultList(routineSectionElement);
    assertTrue(!!resultList);
    return resultList;
  }

  /**
   * Returns the Run Tests button.
   * @return {!CrButtonElement}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(routineSectionElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns the Show/Hide Test Report button.
   * @return {!CrButtonElement}
   */
  function getToggleTestReportButton() {
    const button =
        dx_utils.getToggleTestReportButtonFromSection(routineSectionElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns the learn more help button.
   * @return {!CrButtonElement}
   */
  function getLearnMoreButton() {
    return dx_utils.getLearnMoreButtonFromSection(routineSectionElement);
  }

  /**
   * Returns the status badge.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    return /** @type {!TextBadgeElement} */ (
        routineSectionElement.$$('#testStatusBadge'));
  }

  /**
   * Returns the status text.
   * TODO(joonbug): Update this type for use with assertElementContainsText
   * @return {?Element}
   */
  function getStatusTextElement() {
    return routineSectionElement.$$('#testStatusText');
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {boolean}
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
   * Clicks the show/hide test report button.
   * @return {!Promise}
   */
  function clickToggleTestReportButton() {
    getToggleTestReportButton().click();
    return flushTasks();
  }

  /**
   * Returns an array of the entries in the list.
   * @return {!NodeList<!RoutineResultEntryElement>}
   */
  function getEntries() {
    return dx_utils.getResultEntries(getResultList());
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

  test('ResultListToggleButton', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    // TODO(joonbug): Use visibility assert over testing .hidden attr.
    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertTrue(getResultList().hidden);
          assertTrue(getToggleTestReportButton().hidden);
          return clickRunTestsButton();
        })
        .then(() => {
          // Report is still hidden by default, but toggle button is visible.
          assertTrue(getResultList().hidden);
          assertFalse(getToggleTestReportButton().hidden);
          return clickToggleTestReportButton();
        })
        .then(() => {
          // Report is visible when button is clicked.
          assertFalse(getResultList().hidden);
          assertFalse(getToggleTestReportButton().hidden);
          assertFalse(getLearnMoreButton().hidden);
          return clickToggleTestReportButton();
        })
        .then(() => {
          // Report is hidden when button is clicked again.
          assertTrue(getResultList().hidden);
          assertTrue(getLearnMoreButton().hidden);
          assertFalse(getToggleTestReportButton().hidden);
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

          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be running.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[1].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be completed.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[1].item.progress);
        });
  });

  test('ResultListFiltersBySupported', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineName.kMemory, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineName.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeSupportedRoutines([RoutineName.kMemory]);

    return initializeRoutineSection(routines)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(RoutineName.kMemory, entries[0].item.routine);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(RoutineName.kMemory, entries[0].item.routine);
        });
  });

  test('ResultListStatusSuccess', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineName.kMemory, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertTrue(getStatusBadge().hidden);
          assertTrue(getStatusTextElement().hidden);
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.DEFAULT);
          assertEquals(getStatusBadge().value, 'Test running');

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(getStatusTextElement(), 'kMemory');

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is visible with success.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
          assertEquals(getStatusBadge().value, 'SUCCESS');

          // Text is visible saying test succeeded.
          assertFalse(getStatusTextElement().hidden);
          assertEquals(
              getStatusTextElement().textContent.trim(), 'Test succeeded');
        });
  });

  test('ResultListStatusFail', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kFloatingPoint,
      RoutineName.kCpuCache,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineName.kFloatingPoint, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineName.kCpuCache, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertTrue(getStatusBadge().hidden);
          assertTrue(getStatusTextElement().hidden);
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.DEFAULT);
          assertEquals(getStatusBadge().value, 'Test running');

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'kFloatingPoint');

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is still visible with "test running", even though first one
          // failed.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.DEFAULT);
          assertEquals(getStatusBadge().value, 'Test running');

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'kCpuCache');

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is visible with fail.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.ERROR);
          assertEquals(getStatusBadge().value, 'FAILED');

          // Text is visible saying test failed.
          assertFalse(getStatusTextElement().hidden);
          assertEquals(
              getStatusTextElement().textContent.trim(), 'Test failed');
        });
  });
}
