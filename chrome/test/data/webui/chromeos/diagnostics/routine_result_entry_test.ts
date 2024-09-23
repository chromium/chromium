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
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('routineResultEntryTestSuite', function() {
  let routineResultEntryElement: RoutineResultEntryElement|null = null;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    routineResultEntryElement?.remove();
    routineResultEntryElement = null;
  });

  function initializeRoutineResultEntry(usingRoutineGroups: boolean = false):
      Promise<void> {
    // Add the entry to the DOM.
    routineResultEntryElement = document.createElement(RoutineResultEntryElement.is);
    assert(routineResultEntryElement);
    document.body.appendChild(routineResultEntryElement);
    routineResultEntryElement.usingRoutineGroups = usingRoutineGroups;
    return flushTasks();
  }

  /**
   * Updates the item in the element.
   */
  function updateItem(item: ResultStatusItem|RoutineGroup): Promise<void> {
    assert(routineResultEntryElement);
    routineResultEntryElement.item = item;
    return flushTasks();
  }

  /**
   * Initializes the entry then updates the item.
   */
  function initializeEntryWithItem(
      item: ResultStatusItem|RoutineGroup,
      usingRoutineGroups: boolean = false): Promise<void> {
    return initializeRoutineResultEntry(usingRoutineGroups).then(() => {
      return updateItem(item);
    });
  }

  /**
   * Creates a completed result status item with a result.
   */
  function createCompletedStatus(
      routine: RoutineType, result: RoutineResult): ResultStatusItem {
    const status = new ResultStatusItem(routine, ExecutionProgress.COMPLETED);
    status.result = result;
    return status;
  }

  function getAnnoucedText(): string {
    assert(routineResultEntryElement);
    return routineResultEntryElement.getAnnouncedTextForTesting();
  }

  /**
   * Returns the routine name element text content.
   */
  function getNameText(): string {
    assert(routineResultEntryElement);
    const name =
        routineResultEntryElement.shadowRoot!.querySelector<HTMLDivElement>(
            '#routine');
    assert(name);
    return name!.textContent!.trim();
  }

  /**
   * Returns the status badge content.
   */
  function getStatusBadge(): TextBadgeElement {
    assert(routineResultEntryElement);
    const badge =
        routineResultEntryElement.shadowRoot!.querySelector<TextBadgeElement>(
            '#status');
    assert(badge);
    return badge;
  }

  /**
   * Returns the span wrapping the failure reason text.
   */
  function getFailedTestContainer(): HTMLSpanElement {
    assert(routineResultEntryElement);
    const failedTestContainer =
        routineResultEntryElement.shadowRoot!.querySelector<HTMLSpanElement>(
            '#failed-test-text');
    assert(failedTestContainer);
    return failedTestContainer;
  }

  test('ElementRendered', () => {
    return initializeRoutineResultEntry().then(() => {
      // Verify the element rendered.
      assert(routineResultEntryElement);
      const div =
          routineResultEntryElement.shadowRoot!.querySelector('.entry-row');
      assert(div);
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
    const item = createCompletedStatus(RoutineType.kCpuStress, {
      simpleResult: StandardRoutineResult.kTestPassed,
      powerResult: undefined,
    });
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
    const item = createCompletedStatus(RoutineType.kCpuStress, {
      simpleResult: StandardRoutineResult.kTestFailed,
      powerResult: undefined,
    });
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
    const item = createCompletedStatus(RoutineType.kBatteryCharge, {
      powerResult: {
        simpleResult: StandardRoutineResult.kTestPassed,
        percentChange: 10,
        timeElapsedSeconds: 10,
      },
      simpleResult: undefined,
    });
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
        [{
          routine: RoutineType.kLanConnectivity,
          blocking: false,
        }],
        'lanConnectivityRoutineText');
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

          item = createCompletedStatus(routine, {
            simpleResult: StandardRoutineResult.kTestPassed,
            powerResult: undefined,
          });

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

          item = createCompletedStatus(routine, {
            simpleResult: StandardRoutineResult.kTestFailed,
            powerResult: undefined,
          });
          expectedAnnounceText = 'Lan Connectivity test - FAILED';

          return updateItem(item);
        })
        .then(() => {
          assertEquals(expectedAnnounceText, getAnnoucedText());
        });
  });
});
