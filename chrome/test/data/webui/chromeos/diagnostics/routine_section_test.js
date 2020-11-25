// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_entry.js';
import 'chrome://diagnostics/routine_section.js';

import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {fakePowerRoutineResults, fakeRoutineResults} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {ExecutionProgress} from 'chrome://diagnostics/routine_list_executor.js';
import {BadgeType} from 'chrome://diagnostics/text_badge.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

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
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

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
   * @param {!Array<!RoutineType>} routines
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
    routineSectionElement.isTestRunning = false;

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
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          assertFalse(isRunTestsButtonDisabled());
          assertFalse(routineSectionElement.isTestRunning);
          return clickRunTestsButton();
        })
        .then(() => {
          assertTrue(isRunTestsButtonDisabled());
          assertTrue(routineSectionElement.isTestRunning);
        });
  });

  test('ResultListToggleButton', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
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
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
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
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kMemory,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    routineController.setFakeSupportedRoutines(
        [chromeos.diagnostics.mojom.RoutineType.kMemory]);

    return initializeRoutineSection(routines)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(
              chromeos.diagnostics.mojom.RoutineType.kMemory,
              entries[0].item.routine);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(
              chromeos.diagnostics.mojom.RoutineType.kMemory,
              entries[0].item.routine);
        });
  });

  test('ResultListStatusSuccess', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kMemory,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

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
              getStatusTextElement(),
              loadTimeData.getString('memoryRoutineText'));

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
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
    ];

    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

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
              getStatusTextElement(),
              loadTimeData.getString('cpuFloatingPointAccuracyRoutineText'));

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
              getStatusTextElement(),
              loadTimeData.getString('cpuCacheRoutineText'));

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
