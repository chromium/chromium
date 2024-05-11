// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_section.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {createRoutine} from 'chrome://diagnostics/diagnostics_utils.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {ExecutionProgress, ResultStatusItem, TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {getRoutineType, RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResultListElement} from 'chrome://diagnostics/routine_result_list.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {BadgeType, TextBadgeElement} from 'chrome://diagnostics/text_badge.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('routineSectionTestSuite', function() {
  let routineSectionElement: RoutineSectionElement|null = null;

  const routineController = new FakeSystemRoutineController();

  const originalTime: () => number = performance.now;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Setup a fake routine controller so that nothing resolves unless
    // done explicitly.
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  teardown(function() {
    routineSectionElement?.remove();
    routineSectionElement = null;
  });

  /**
   * Initializes the element and sets the routines.
   */
  function initializeRoutineSection(
      routines: RoutineType[]|RoutineGroup[], runtime = 1) {
    // Add the entry to the DOM.
    routineSectionElement = document.createElement('routine-section');
    assert(routineSectionElement);
    document.body.appendChild(routineSectionElement);

    // Assign the routines to the property.
    routineSectionElement.routines = routines;
    routineSectionElement.testSuiteStatus = TestSuiteStatus.NOT_RUNNING;
    routineSectionElement.routineRuntime = runtime;

    if (!(routines[0] instanceof RoutineGroup) && routines.length === 1 && [
          RoutineType.kBatteryDischarge,
          RoutineType.kBatteryCharge,
        ].includes(routines[0] as RoutineType)) {
      routineSectionElement.isPowerRoutine = true;
    }

    return flushTasks();
  }

  /**
   * Returns the result list element.
   */
  function getResultList(): RoutineResultListElement {
    const resultList = dx_utils.getResultList(routineSectionElement);
    assert(resultList);
    return resultList;
  }

  /**
   * Returns the Run Tests button.
   */
  function getRunTestsButton(): CrButtonElement {
    const button = dx_utils.getRunTestsButtonFromSection(routineSectionElement);
    assert(button);
    return button;
  }

  /**
   * Returns the Stop tests button.
   */
  function getStopTestsButton(): CrButtonElement {
    const button =
        dx_utils.getStopTestsButtonFromSection(routineSectionElement);
    assert(button);
    return button;
  }

  /**
   * Returns the Show/Hide Test Report button.
   */
  function getToggleTestReportButton(): CrButtonElement {
    const button =
        dx_utils.getToggleTestReportButtonFromSection(routineSectionElement);
    assert(button);
    return button;
  }

  /**
   * Returns the status badge.
   */
  function getStatusBadge(): TextBadgeElement {
    assert(routineSectionElement);
    const testStatusBadge =
        routineSectionElement.shadowRoot!.querySelector<TextBadgeElement>(
            '#testStatusBadge');
    assert(testStatusBadge);
    return testStatusBadge;
  }

  /**
   * Returns the status text.
   */
  function getStatusTextElement(): HTMLSpanElement {
    assert(routineSectionElement);
    const statusText =
        routineSectionElement.shadowRoot!.querySelector<HTMLSpanElement>(
            '#testStatusText');
    assert(statusText);
    return statusText;
  }

  /**
   * Returns whether the run tests button is disabled.
   */
  function isRunTestsButtonDisabled(): boolean {
    return getRunTestsButton().disabled;
  }

  /**
   * Clicks the run tests button.
   */
  function clickRunTestsButton(): Promise<void> {
    getRunTestsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the stop tests button.
   */
  function clickStopTestsButton(): Promise<void> {
    getStopTestsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the show/hide test report button.
   */
  function clickToggleTestReportButton(): Promise<void> {
    getToggleTestReportButton().click();
    return flushTasks();
  }

  function getAnnouncedText(): string {
    assert(routineSectionElement);
    return routineSectionElement.announcedText;
  }

  /**
   * Returns an array of the entries in the list.
   */
  function getEntries(): NodeListOf<RoutineResultEntryElement> {
    return dx_utils.getResultEntries(getResultList());
  }

  /**
   * Returns whether the "result list" section is expanded or not.
   */
  function isIronCollapseOpen(): boolean {
    assert(routineSectionElement);
    return routineSectionElement.$.collapse.opened;
  }

  /**
   * Get currentTestName private member for testing.
   */
  function getCurrentTestName(): string {
    assert(routineSectionElement);
    return routineSectionElement.currentTestName;
  }

  /**
   * @param t Set current time to t.
   */
  function setMockTime(t: number): void {
    performance.now = () => t;
  }

  /**
   * Restores mocked time to the original function.
   */
  function resetMockTime(): void {
    performance.now = originalTime;
  }

  /**
   * Updates time-to-finish status
   */
  function triggerStatusUpdate(): Promise<void> {
    assert(routineSectionElement);
    routineSectionElement.setRunningStatusBadgeText();
    return flushTasks();
  }

  function setIsActive(isActive: boolean): Promise<void> {
    assert(routineSectionElement);
    routineSectionElement.isActive = isActive;
    return flushTasks();
  }

  function setRoutines(routines: RoutineType[]): Promise<void> {
    assert(routineSectionElement);
    routineSectionElement.routines = routines;
    return flushTasks();
  }

  function setHideRoutineStatus(hideRoutineStatus: boolean): Promise<void> {
    assert(routineSectionElement);
    routineSectionElement.hideRoutineStatus = hideRoutineStatus;
    return flushTasks();
  }

  function getLearnMoreButton(): CrButtonElement {
    assert(routineSectionElement);
    const learnMoreButton =
        routineSectionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#learnMoreButton');
    assert(learnMoreButton);
    return learnMoreButton;
  }

  test('ElementRenders', () => {
    return initializeRoutineSection([]).then(() => {
      // Verify the element rendered.
      assertTrue(!!routineSectionElement!.shadowRoot!.querySelector(
          '#routineSection'));
    });
  });

  test('ElementVisibleWhenRoutinesLengthGreaterThanZero', () => {
    return initializeRoutineSection([])
        .then(() => {
          assert(routineSectionElement);
          // Verify the element is hidden.
          assertFalse(isVisible(routineSectionElement.shadowRoot!.querySelector(
              '#routineSection')));
        })
        .then(() => setRoutines([RoutineType.kLanConnectivity]))
        .then(() => {
          assert(routineSectionElement);
          // Verify the element is not hidden.
          assertTrue(isVisible(routineSectionElement.shadowRoot!.querySelector(
              '#routineSection')));
        });
  });

  test('ClickButtonShowsStopTest', () => {
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          assert(routineSectionElement);
          assertFalse(isRunTestsButtonDisabled());
          assertEquals(
              TestSuiteStatus.NOT_RUNNING,
              routineSectionElement.testSuiteStatus);
          return clickRunTestsButton();
        })
        .then(() => {
          assert(routineSectionElement);
          assertFalse(isVisible(getRunTestsButton()));
          assertTrue(isVisible(getStopTestsButton()));
          assertEquals(
              TestSuiteStatus.RUNNING, routineSectionElement.testSuiteStatus);
          dx_utils.assertElementContainsText(
              getStopTestsButton(),
              loadTimeData.getString('stopTestButtonText'));
        });
  });

  test('ResultListToggleButton', () => {
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
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
    const routines = [
      RoutineType.kBatteryCharge,
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
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
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
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);

          // Second routine is not started.
          assertEquals(
              routines[1], (entries[1]!.item as ResultStatusItem).routine);
          assertEquals(
              ExecutionProgress.NOT_STARTED, entries[1]!.item.progress);

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
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.COMPLETED, entries[0]!.item.progress);

          // Second routine should be running.
          assertEquals(
              routines[1], (entries[1]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.RUNNING, entries[1]!.item.progress);

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
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.COMPLETED, entries[0]!.item.progress);

          // Second routine should be completed.
          assertEquals(
              routines[1], (entries[1]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.COMPLETED, entries[1]!.item.progress);
        });
  });

  test('ResultListFiltersBySupported', () => {
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeSupportedRoutines([RoutineType.kMemory]);

    return initializeRoutineSection(routines)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(
              RoutineType.kMemory,
              (entries[0]!.item as ResultStatusItem).routine);
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
              RoutineType.kMemory,
              (entries[0]!.item as ResultStatusItem).routine);
        });
  });

  test('ResultListStatusSuccess', () => {
    const routines = [
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

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
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

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
          assertEquals(getStatusBadge().value, 'PASSED');

          // Text is visible saying test succeeded.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Test succeeded');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
        });
  });

  test('PowerTestResultListStatusSuccess', () => {
    const routines = [
      RoutineType.kBatteryCharge,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kBatteryCharge, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isVisible(getStatusBadge()));
          assertFalse(isVisible(getStatusTextElement()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('batteryChargeRoutineText').toLowerCase());

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Text is visible saying test progress.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Charged 0.00% in 0 seconds.');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
        });
  });

  test('ResultListStatusFail', () => {
    const routines = [
      RoutineType.kCpuFloatingPoint,
      RoutineType.kCpuCache,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuFloatingPoint, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);

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
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

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
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

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
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be running.
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);

          // Second routine is not started.
          assertEquals(
              routines[1], (entries[1]!.item as ResultStatusItem).routine);
          assertEquals(
              ExecutionProgress.NOT_STARTED, entries[1]!.item.progress);
          // // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // First routine should be completed.
          assertEquals(ExecutionProgress.COMPLETED, entries[0]!.item.progress);

          // Second routine should be running.
          assertEquals(ExecutionProgress.RUNNING, entries[1]!.item.progress);
        })
        .then(() => clickStopTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should still be completed.
          assertEquals(ExecutionProgress.COMPLETED, entries[0]!.item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.CANCELLED, entries[1]!.item.progress);

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
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be running.
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);

          // Second routine is not started.
          assertEquals(
              routines[1], (entries[1]!.item as ResultStatusItem).routine);
          assertEquals(
              ExecutionProgress.NOT_STARTED, entries[1]!.item.progress);
        })
        // Stop running test.
        .then(() => clickStopTestsButton())
        .then(() => {
          // Badge and status are still visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be cancelled.
          assertEquals(ExecutionProgress.CANCELLED, entries[0]!.item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.CANCELLED, entries[1]!.item.progress);

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
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

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
          assertEquals(ExecutionProgress.CANCELLED, entries[0]!.item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.CANCELLED, entries[1]!.item.progress);

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
      RoutineType.kCpuCache,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);

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
          assertEquals(
              routines[0], (entries[0]!.item as ResultStatusItem).routine);
          assertEquals(ExecutionProgress.COMPLETED, entries[0]!.item.progress);

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
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);

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
      RoutineType.kCpuCache,
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
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
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
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

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
              getStatusBadge().value,
              loadTimeData.getStringF('routineRemainingMin', '2'));

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
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

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
              getStatusBadge().value,
              loadTimeData.getStringF('routineRemainingMin', '20'));

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

  test('PageChangeStopsRunningTest', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [RoutineType.kMemory];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('memoryRoutineText').toLowerCase());

          // Simulate a navigation page change event.
          return setIsActive(false);
        })
        .then(() => flushTasks())
        .then(() => {
          // Result list is no longer visible.
          assertFalse(isVisible(getResultList()));
          // Memory routine should be cancelled.
          assertEquals(
              ExecutionProgress.CANCELLED, getEntries()[0]!.item.progress);
        });
  });

  test('RoutineStatusAndActionsHidden', () => {
    return initializeRoutineSection([])
        .then(() => setHideRoutineStatus(true))
        .then(() => {
          assert(routineSectionElement);
          assertFalse(isVisible(getLearnMoreButton()));
          assertFalse(isVisible(routineSectionElement.shadowRoot!.querySelector(
              '.routine-status-container')));
          assertFalse(isVisible(routineSectionElement.shadowRoot!.querySelector(
              '.button-container')));
        });
  });


  test('StopAfterFirstBlockingFailureInRoutineGroup', () => {
    const localNetworkGroup = new RoutineGroup(
        [
          createRoutine(RoutineType.kGatewayCanBePinged, true),
          createRoutine(RoutineType.kLanConnectivity, true),
        ],
        'localNetworkGroupLabel');

    const nameResolutionGroup = new RoutineGroup(
        [createRoutine(RoutineType.kDnsResolverPresent, true)],
        'nameResolutionGroupLabel');
    const groups = [localNetworkGroup, nameResolutionGroup];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kGatewayCanBePinged, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kLanConnectivity, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();

          // First routine should be running.
          assertEquals(
              RoutineType.kGatewayCanBePinged,
              (entries[0]!.item as RoutineGroup).routines[0]);
          assertEquals(
              ExecutionProgress.RUNNING,
              (entries[0]!.item as RoutineGroup).progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();

          // Second routine in the first group should be running.
          assertEquals(
              RoutineType.kLanConnectivity,
              (entries[0]!.item as RoutineGroup).routines[1]);
          assertEquals(
              ExecutionProgress.RUNNING,
              (entries[0]!.item as RoutineGroup).progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          const entries = getEntries();
          // We've encountered a test failure which means we should no longer
          // update the status of our remaining routine result entries.
          assertTrue(getResultList().ignoreRoutineStatusUpdates);

          // Second routine in the first group should have completed.

          assertEquals(
              RoutineType.kLanConnectivity,
              (entries[0]!.item as RoutineGroup).routines[1]);
          assertEquals(
              ExecutionProgress.COMPLETED,
              (entries[0]!.item as RoutineGroup).progress);

          // Text badge should display 'FAILED' for the first group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'FAILED');

          // Remaining routine groups should display the skipped state.
          assertEquals(ExecutionProgress.SKIPPED, entries[1]!.item.progress);

          // Remaining routine should still be running in the background.
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.RUNNING);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          // All tests are completed and the ignore updates flag should be off
          // again.
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.COMPLETED);
          assertFalse(getResultList().ignoreRoutineStatusUpdates);
        });
  });

  test('NonBlockingRoutineFailureHandledCorrectly', () => {
    const localNetworkGroup = new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel');

    const nameResolutionGroup = new RoutineGroup(
        [createRoutine(RoutineType.kDnsResolverPresent, true)],
        'nameResolutionGroupLabel');
    const groups = [localNetworkGroup, nameResolutionGroup];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();

          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength,
              (entries[0]!.item as RoutineGroup).routines[0]);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertFalse(getResultList().ignoreRoutineStatusUpdates);

          // Second routine in the first group should still be running
          // despite the |kSignalStrength| routine failure.
          assertEquals(
              RoutineType.kCaptivePortal,
              (entries[0]!.item as RoutineGroup).routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(
                  (entries[0]!.item as RoutineGroup).routines[1] as
                  RoutineType));

          // Text badge should display 'WARNING' for the first group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'WARNING');

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();

          // Text badge should still display 'WARNING' for the first group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'WARNING');

          // First routine in the second group should be running.
          assertEquals(
              RoutineType.kDnsResolverPresent,
              (entries[1]!.item as RoutineGroup).routines[0]);
          assertEquals(
              ExecutionProgress.RUNNING,
              (entries[1]!.item as RoutineGroup).progress);


          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          const entries = getEntries();

          // Text badge should display 'PASSED' for the second group.
          const textBadge = entries[1]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'PASSED');
          assertEquals(ExecutionProgress.COMPLETED, entries[1]!.item.progress);
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.COMPLETED);
        });
  });

  test('MultipleNonBlockingTestsFail', () => {
    const groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength,
              (entries[0]!.item as RoutineGroup).routines[0]);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertFalse(getResultList().ignoreRoutineStatusUpdates);
          // Second routine in the first group should still be running
          // despite the |kSignalStrength| routine failure.
          assertEquals(
              RoutineType.kCaptivePortal,
              (entries[0]!.item as RoutineGroup).routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(
                  (entries[0]!.item as RoutineGroup).routines[1] as
                  RoutineType));
          // Text badge should display 'WARNING' for the first group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'WARNING');
          // Failed test text should be set properly.
          assertEquals(
              (entries[0]!.item as RoutineGroup).failedTest,
              RoutineType.kSignalStrength);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // Text badge should still display 'WARNING' for the first group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'WARNING');
          // Failed test does not get overwritten.
          assertEquals(
              (entries[0]!.item as RoutineGroup).failedTest,
              RoutineType.kSignalStrength);
        });
  });

  test('LastNonBlockingRoutineInGroupFails', () => {
    const groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength,
              (entries[0]!.item as RoutineGroup).routines[0]);
          assertEquals(ExecutionProgress.RUNNING, entries[0]!.item.progress);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertEquals(
              RoutineType.kCaptivePortal,
              (entries[0]!.item as RoutineGroup).routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(
                  (entries[0]!.item as RoutineGroup).routines[1] as
                  RoutineType));
          // Text badge should display 'RUNNING' for the first group since
          // the signal strength test passed but we still have unfinished
          // routines in this group.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'RUNNING');
          // Failed test text should be unset.
          assertFalse(!!(entries[0]!.item as RoutineGroup).failedTest);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // Text badge should display 'WARNING' despite being in a completed
          // state.
          const textBadge = entries[0]!.shadowRoot!.querySelector('#status');
          assert(textBadge);
          dx_utils.assertElementContainsText(
              textBadge.shadowRoot!.querySelector('#textBadge'), 'WARNING');
          assertEquals(entries[0]!.item.progress, ExecutionProgress.COMPLETED);
          // Failed test text should be set properly.
          assertEquals(
              (entries[0]!.item as RoutineGroup).failedTest,
              RoutineType.kCaptivePortal);
        });
  });

  test('AnnounceOnAllTestPassed', () => {
    const groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.COMPLETED);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('AnnounceOnAllNonBlockingTestPassed', () => {
    const groups = [
      new RoutineGroup(
          [
            createRoutine(RoutineType.kCaptivePortal, false),
          ],
          'wifiGroupLabel'),
      new RoutineGroup(
          [
            createRoutine(RoutineType.kDnsResolverPresent, true),
          ],
          'wifiGroupLabel'),
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.COMPLETED);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('AnnounceOnBlockingTestFailed', () => {
    const groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ true),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assert(routineSectionElement);
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.COMPLETED);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('NoAnnounceOnBlockingTestCancelled', () => {
    const groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ true),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return clickStopTestsButton();
        })
        .then(() => {
          assert(routineSectionElement);
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.NOT_RUNNING);
          assertEquals('', getAnnouncedText());
        });
  });
});
