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
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function routineSectionTestSuite() {
  /** @type {?RoutineSectionElement} */
  let routineSectionElement = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {function(this:Performance): number} */
  const originalTime = performance.now;

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
   * @param {number=} runtime in minutes.
   */
  function initializeRoutineSection(routines, runtime = 1) {
    assertFalse(!!routineSectionElement);

    // Add the entry to the DOM.
    routineSectionElement = /** @type {!RoutineSectionElement} */ (
        document.createElement('routine-section'));
    assertTrue(!!routineSectionElement);
    document.body.appendChild(routineSectionElement);

    // Assign the routines to the property.
    routineSectionElement.routines = routines;
    routineSectionElement.isTestRunning = false;
    routineSectionElement.routineRuntime = runtime;

    if (routines.length === 1 && [
          chromeos.diagnostics.mojom.RoutineType.kBatteryDischarge,
          chromeos.diagnostics.mojom.RoutineType.kBatteryCharge
        ].includes(routines[0])) {
      routineSectionElement.isPowerRoutine = true;
    }

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
   * Returns the Stop tests button.
   * @return {!CrButtonElement}
   */
  function getStopTestsButton() {
    const button =
        dx_utils.getStopTestsButtonFromSection(routineSectionElement);
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
   * Returns the status badge.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    return /** @type {!TextBadgeElement} */ (
        routineSectionElement.$$('#testStatusBadge'));
  }

  /**
   * Returns the status text.
   * @return {!HTMLElement}
   */
  function getStatusTextElement() {
    const statusText =
        /** @type {!HTMLElement} */ (
            routineSectionElement.$$('#testStatusText'));
    assertTrue(!!statusText);
    return statusText;
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
   * Clicks the stop tests button.
   * @return {!Promise}
   */
  function clickStopTestsButton() {
    getStopTestsButton().click();
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

  /**
   * Returns whether the "result list" section is expanded or not.
   * @return {boolean}
   */
  function isIronCollapseOpen() {
    return routineSectionElement.$.collapse.opened;
  }

  /**
   * Get currentTestName_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {string}
   */
  function getCurrentTestName() {
    assertTrue(!!routineSectionElement);
    return routineSectionElement.currentTestName_;
  }

  /**
   * @param {number} t Set current time to t.
   */
  function setMockTime(t) {
    performance.now = () => t;
  }

  /**
   * Restores mocked time to the original function.
   */
  function resetMockTime() {
    performance.now = originalTime;
  }

  /**
   * Updates time-to-finish status
   * @suppress {visibility} // access private member for test
   * @return {!Promise}
   */
  function triggerStatusUpdate() {
    routineSectionElement.setRunningStatusBadgeText_();
    return flushTasks();
  }

  test('ElementRenders', () => {
    return initializeRoutineSection([]).then(() => {
      // Verify the element rendered.
      assertTrue(!!routineSectionElement.$$('#routineSection'));
    });
  });

  test('ClickButtonShowsStopTest', () => {
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
          assertFalse(isVisible(getRunTestsButton()));
          assertTrue(isVisible(getStopTestsButton()));
          assertTrue(routineSectionElement.isTestRunning);
          dx_utils.assertElementContainsText(
              getStopTestsButton(),
              loadTimeData.getString('stopTestButtonText'));
        });
  });

  test('ResultListToggleButton', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Report and toggle button are visible.
          assertTrue(isIronCollapseOpen());
          assertTrue(isVisible(getToggleTestReportButton()));
          return clickToggleTestReportButton();
        })
        .then(() => {
          // Report is hidden when button is clicked again.
          assertFalse(isIronCollapseOpen());
          assertTrue(isVisible(getToggleTestReportButton()));
        });
  });

  test('PowerResultListToggleButton', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kBatteryCharge,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Report is hidden by default and so is toggle button.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
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
          assertFalse(isVisible(getStatusBadge()));
          assertFalse(isVisible(getStatusTextElement()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value, loadTimeData.getString('testRunning'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('memoryRoutineText').toLowerCase());

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
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Test succeeded');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
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
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value, loadTimeData.getString('testRunning'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('cpuFloatingPointAccuracyRoutineText')
                  .toLowerCase());

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
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value, loadTimeData.getString('testRunning'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('cpuCacheRoutineText').toLowerCase());

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
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Test failed');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
        });
  });

  test('CancelQueuedRoutinesWithRoutineCompleted', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);
          // // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // First routine should be completed.
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be running.
          assertEquals(ExecutionProgress.kRunning, entries[1].item.progress);
        })
        .then(() => clickStopTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should still be completed.
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          // Badge shows that test was stopped.
          assertEquals(getStatusBadge().badgeType, BadgeType.STOPPED);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('testStoppedBadgeText'));

          // Status text shows test that was cancelled.
          assertTrue(isVisible(getStatusTextElement()));
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
        });
  });

  test('CancelRunningAndQueuedRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);
        })
        // Stop running test.
        .then(() => clickStopTestsButton())
        .then(() => {
          // Badge and status are still visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Status text shows test that was cancelled.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
        });
  });

  test('RunAgainShownAfterCancellation', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuStress,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        // Start tests.
        .then(() => clickRunTestsButton())
        // Stop running test.
        .then(() => clickStopTestsButton())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Status text shows test that was cancelled.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
          // Button is visible and text shows "Run again"
          assertTrue(isVisible(getRunTestsButton()));
          dx_utils.assertElementContainsText(
              getRunTestsButton(),
              loadTimeData.getString('runAgainButtonText'));
        });
  });

  test('RunTestsMultipleTimes', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
    ];
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => routineController.resolveRoutineForTesting())
        .then(() => flushTasks())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Status text shows that a routine succeeded.
          dx_utils.assertElementContainsText(
              getStatusTextElement(), loadTimeData.getString('testSuccess'));
          // Button is visible and text shows "Run again"
          assertTrue(isVisible(getRunTestsButton()));
          dx_utils.assertElementContainsText(
              getRunTestsButton(),
              loadTimeData.getString('runAgainButtonText'));
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be running.
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Button text should be "Stop test"
          dx_utils.assertElementContainsText(
              getStopTestsButton(),
              loadTimeData.getString('stopTestButtonText'));

          // Status text shows test that is running.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'routineNameText', getCurrentTestName().toLowerCase()));
        });
  });

  test('ReportButtonHiddenWithSingleRoutine', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
    ];
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertFalse(isVisible(getToggleTestReportButton()));
        });
  });

  test('ReportButtonShownWithMultipleRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
      chromeos.diagnostics.mojom.RoutineType.kCpuStress,
    ];
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertTrue(isVisible(getToggleTestReportButton()));
        });
  });

  test('RoutineRuntimeStatus', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kMemory,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    setMockTime(0);

    return initializeRoutineSection(routines, 2)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertTrue(isVisible(getStatusBadge()));
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value, loadTimeData.getString('testRunning'));

          return triggerStatusUpdate();
        })
        .then(() => {
          dx_utils.assertTextContains(getStatusBadge().value, '2');

          setMockTime(110000);  // fast forward time to 110 seconds
          return triggerStatusUpdate();
        })
        .then(() => {
          // Display 'less than a minute remaining'
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          resetMockTime();
        });
  });

  test('RoutineRuntimeStatusLarge', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kMemory,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);

    setMockTime(0);

    return initializeRoutineSection(routines, 20)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertTrue(isVisible(getStatusBadge()));
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value, loadTimeData.getString('testRunning'));

          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 20 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '20');

          setMockTime(120000);  // set time to 120 seconds
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should still say about 20 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '20');

          setMockTime(1020000);  // set time to 17 minutes
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 5 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '5');

          setMockTime(1500000);  // set time to 25 minutes (past estimate)
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 5 minutes remaining, even after estimated runtime.
          dx_utils.assertTextContains(getStatusBadge().value, '5');

          // Should update status to just a few more minutes..
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('routineRemainingMinFinalLarge'));
          resetMockTime();
        });
  });
}
