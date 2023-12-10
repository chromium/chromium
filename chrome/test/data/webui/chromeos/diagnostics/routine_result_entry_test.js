// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_entry.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResult, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {BadgeType, TextBadgeElement} from 'chrome://diagnostics/text_badge.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('routineResultEntryTestSuite', function() {
  /** @type {?RoutineResultEntryElement} */
  let routineResultEntryElement = null;

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(function() {
    if (routineResultEntryElement) {
      routineResultEntryElement.remove();
    }
    routineResultEntryElement = null;
  });

  /** @param {boolean=} usingRoutineGroups */
  function initializeRoutineResultEntry(usingRoutineGroups = false) {
    assertFalse(!!routineResultEntryElement);

    // Add the entry to the DOM.
    routineResultEntryElement = /** @type {!RoutineResultEntryElement} */ (
        document.createElement('routine-result-entry'));
    assertTrue(!!routineResultEntryElement);
    document.body.appendChild(routineResultEntryElement);
    routineResultEntryElement.usingRoutineGroups = usingRoutineGroups;
    return flushTasks();
  }

  /**
   * Updates the item in the element.
   * @param {ResultStatusItem|RoutineGroup} item
   * @return {!Promise}
   */
  function updateItem(item) {
    routineResultEntryElement.item = item;
    return flushTasks();
  }

  /**
   * Initializes the entry then updates the item.
   * @param {ResultStatusItem|RoutineGroup} item
   * @param {boolean=} usingRoutineGroups
   * @return {!Promise}
   */
  function initializeEntryWithItem(item, usingRoutineGroups = false) {
    return initializeRoutineResultEntry(usingRoutineGroups).then(() => {
      return updateItem(item);
    });
  }

  /**
   * Creates a completed result status item with a result.
   * @param {!RoutineType} routine
   * @param {!RoutineResult} result
   * @return {!ResultStatusItem}
   */
  function createCompletedStatus(routine, result) {
    const status = new ResultStatusItem(routine, ExecutionProgress.COMPLETED);
    status.result = result;
    return status;
  }

  /**
   * @suppress {visibility}
   * @return {string}
   */
  function getAnnoucedText() {
    assertTrue(!!routineResultEntryElement);

    return routineResultEntryElement.announcedText;
  }

  /**
   * Returns the routine name element text content.
   * @return {string}
   */
  function getNameText() {
    const name = routineResultEntryElement.shadowRoot.querySelector('#routine');
    assertTrue(!!name);
    return name.textContent.trim();
  }

  /**
   * Returns the status badge content.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    const badge = /** @type{!TextBadgeElement} */ (
        routineResultEntryElement.shadowRoot.querySelector('#status'));
    assertTrue(!!badge);
    return badge;
  }

  /**
   * Returns the span wrapping the failure reason text.
   * @return {!HTMLSpanElement}
   */
  function getFailedTestContainer() {
    const failedTestContainer = /** @type {!HTMLSpanElement} */ (
        routineResultEntryElement.shadowRoot.querySelector(
            '#failed-test-text'));
    assertTrue(!!failedTestContainer);
    return failedTestContainer;
  }

  test('ElementRendered', () => {
    return initializeRoutineResultEntry().then(() => {
      // Verify the element rendered.
      const div =
          routineResultEntryElement.shadowRoot.querySelector('.entry-row');
      assertTrue(!!div);
    });
  });

  test('NotStartedTest', () => {
    const item = new ResultStatusItem(RoutineType.kCpuStress);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should be queued if the test is not started.
      assertTrue(isVisible(getStatusBadge()));
      assertEquals(getStatusBadge().badgeType, BadgeType.QUEUED);
      dx_utils.assertTextContains(
          getStatusBadge().value,
          loadTimeData.getString('testQueuedBadgeText'));
    });
  });

  test('RunningTest', () => {
    const item =
        new ResultStatusItem(RoutineType.kCpuStress, ExecutionProgress.RUNNING);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should be running.
      dx_utils.assertTextContains(
          getStatusBadge().value,
          loadTimeData.getString('testRunningBadgeText'));
      assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
    });
  });

  test('PassedTest', () => {
    const item = createCompletedStatus(
        RoutineType.kCpuStress,
        /** @type {!RoutineResult} */ ({
          simpleResult: StandardRoutineResult.kTestPassed,
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'PASSED');
      assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
    });
  });

  test('FailedTest', () => {
    const item = createCompletedStatus(
        RoutineType.kCpuStress,
        /** @type {!RoutineResult} */ ({
          simpleResult: StandardRoutineResult.kTestFailed,
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'FAILED');
      assertEquals(getStatusBadge().badgeType, BadgeType.ERROR);
    });
  });

  test('StoppedTest', () => {
    const item = new ResultStatusItem(
        RoutineType.kCpuStress, ExecutionProgress.CANCELLED);
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('cpuStressRoutineText')));

      // Status should show that the test was stopped.
      assertEquals(
          getStatusBadge().value,
          loadTimeData.getString('testStoppedBadgeText'));
      assertEquals(getStatusBadge().badgeType, BadgeType.STOPPED);
    });
  });

  test('PowerTest', () => {
    const item = createCompletedStatus(
        RoutineType.kBatteryCharge,
        /** @type {!RoutineResult} */ ({
          powerResult: {
            simpleResult: StandardRoutineResult.kTestPassed,
            isCharging: true,
            percentDelta: 10,
            timeDeltaSeconds: 10,
          },
        }));
    return initializeEntryWithItem(item).then(() => {
      assertEquals(
          getNameText(),
          loadTimeData.getStringF(
              'routineEntryText',
              loadTimeData.getString('batteryChargeRoutineText')));

      // Status should show the passed result.
      assertEquals(getStatusBadge().value, 'PASSED');
      assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
    });
  });

  test('NetworkRoutineHasCorrectFailureMessage', () => {
    const item = new RoutineGroup(
        [RoutineType.kLanConnectivity], 'lanConnectivityRoutineText');
    item.failedTest = RoutineType.kLanConnectivity;
    return initializeEntryWithItem(item, true).then(() => {
      // Span should not be hidden
      assertTrue(isVisible(getFailedTestContainer()));
      dx_utils.assertElementContainsText(
          getFailedTestContainer(),
          loadTimeData.getString('lanConnectivityFailedText'));
    });
  });

  test('AnnouncesForRunningAndFailure', () => {
    const routine = RoutineType.kLanConnectivity;
    let item = new ResultStatusItem(routine, ExecutionProgress.NOT_STARTED);
    let expectedAnnounceText = '';

    return initializeEntryWithItem(item)
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = createCompletedStatus(
              routine, /* @type {!RoutineResult} */ ({
                simpleResult: StandardRoutineResult.kTestPassed,
              }));

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = new ResultStatusItem(routine, ExecutionProgress.SKIPPED);

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = new ResultStatusItem(routine, ExecutionProgress.CANCELLED);

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = new ResultStatusItem(routine, ExecutionProgress.WARNING);

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = new ResultStatusItem(routine, ExecutionProgress.RUNNING);
          expectedAnnounceText = 'Lan Connectivity test - RUNNING';

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());

          item = createCompletedStatus(
              routine, /* @type {!RoutineResult} */ ({
                simpleResult: StandardRoutineResult.kTestFailed,
              }));
          expectedAnnounceText = 'Lan Connectivity test - FAILED';

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());
        });
  });
});
