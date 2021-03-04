// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/battery_status_card.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, ExternalPowerSource} from 'chrome://diagnostics/diagnostics_types.js';
import {getDiagnosticsIcon} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeBatteryChargeStatus, fakeBatteryChargeStatus2, fakeBatteryHealth, fakeBatteryHealth2, fakeBatteryInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {mojoString16ToString} from 'chrome://diagnostics/mojo_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isChildVisible, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

const BATTERY_ICON_PREFIX = 'battery-';

export function batteryStatusCardTestSuite() {
  /** @type {?BatteryStatusCardElement} */
  let batteryStatusElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (batteryStatusElement) {
      batteryStatusElement.remove();
    }
    batteryStatusElement = null;

    provider.reset();
  });

  /**
   * @param {!BatteryInfo} batteryInfo
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatus
   * @param {!Array<!BatteryHealth>} batteryHealth
   * @return {!Promise}
   */
  function initializeBatteryStatusCard(
      batteryInfo, batteryChargeStatus, batteryHealth) {
    assertFalse(!!batteryStatusElement);

    // Initialize the fake data.
    provider.setFakeBatteryChargeStatus(batteryChargeStatus);
    provider.setFakeBatteryHealth(batteryHealth);
    provider.setFakeBatteryInfo(batteryInfo);

    // Add the battery status card to the DOM.
    batteryStatusElement = /** @type {!BatteryStatusCardElement} */ (
        document.createElement('battery-status-card'));
    assertTrue(!!batteryStatusElement);
    document.body.appendChild(batteryStatusElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   * @return {!RoutineSectionElement}
   */
  function getRoutineSection() {
    const routineSection = /** @type {!RoutineSectionElement} */ (
        batteryStatusElement.$$('routine-section'));
    assertTrue(!!routineSection);
    return routineSection;
  }

  /**
   * Returns the status badge.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    const routineSectionElement = getRoutineSection();

    return /** @type {!TextBadgeElement} */ (
        routineSectionElement.$$('#testStatusBadge'));
  }

  /**
   * Returns the status text.
   * @return {!HTMLElement}
   */
  function getStatusTextElement() {
    const routineSectionElement = getRoutineSection();

    const statusText =
        /** @type {!HTMLElement} */ (
            routineSectionElement.$$('#testStatusText'));
    assertTrue(!!statusText);
    return statusText;
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   * @return {!CrButtonElement}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {boolean}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
  }

  /**
   * Get batteryChargeStatus_.powerAdapterStatus private member for testing.
   * @suppress {visibility} // access private member
   * @return {!ExternalPowerSource}
   */
  function getPowerAdapterStatus() {
    assertTrue(!!batteryStatusElement);
    return batteryStatusElement.batteryChargeStatus_.powerAdapterStatus;
  }

  test('BatteryStatusCardPopulated', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          const dataPoints =
              dx_utils.getDataPointElements(batteryStatusElement);
          dx_utils.assertTextContains(
              dataPoints[0].value,
              `${fakeBatteryHealth[0].batteryWearPercentage}`);
          dx_utils.assertTextContains(
              dataPoints[0].tooltipText,
              loadTimeData.getString('batteryHealthTooltipText'));
          assertEquals(fakeBatteryHealth[0].cycleCount, dataPoints[1].value);
          dx_utils.assertTextContains(
              dataPoints[1].tooltipText,
              loadTimeData.getString('cycleCountTooltipText'));
          dx_utils.assertTextContains(
              dataPoints[2].value,
              `${fakeBatteryChargeStatus[0].currentNowMilliamps}`);
          dx_utils.assertTextContains(
              dataPoints[2].tooltipText,
              loadTimeData.getString('currentNowTooltipText'));
          dx_utils.assertElementContainsText(
              batteryStatusElement.$$('#batteryStatusChipInfo'),
              `${fakeBatteryHealth[0].chargeFullDesignMilliampHours}`);
          const barChart =
              dx_utils.getPercentBarChartElement(batteryStatusElement);
          assertEquals(
              fakeBatteryHealth[0].chargeFullNowMilliampHours, barChart.max);
          assertEquals(
              fakeBatteryChargeStatus[0].chargeNowMilliampHours,
              barChart.value);

          // Verify that the data points container is visible.
          const diagnosticsCard =
              dx_utils.getDiagnosticsCard(batteryStatusElement);
          assertTrue(isChildVisible(diagnosticsCard, '.data-points'));

          // Verify the routine section is in the card.
          assertTrue(!!getRoutineSection());
          assertTrue(!!getRunTestsButton());
          assertFalse(isRunTestsButtonDisabled());
        });
  });

  test('PowerRoutine', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(routineSectionElement.routines.length, 1);
          assertEquals(
              routineSectionElement.routines[0],
              chromeos.diagnostics.mojom.RoutineType.kBatteryCharge);

          batteryStatusElement.onBatteryChargeStatusUpdated(
              fakeBatteryChargeStatus[2]);
          return flushTasks();
        })
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(routineSectionElement.routines.length, 1);
          assertEquals(
              routineSectionElement.routines[0],
              chromeos.diagnostics.mojom.RoutineType.kBatteryDischarge);

          batteryStatusElement.onBatteryChargeStatusUpdated(
              fakeBatteryChargeStatus[3]);
          return flushTasks();
        })
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(
              routineSectionElement.additionalMessage,
              loadTimeData.getString('batteryChargeTestFullMessage'));
          assertTrue(isRunTestsButtonDisabled());
          assertTrue(isVisible(/** @type {!HTMLElement} */ (
              routineSectionElement.$$('#messageIcon'))));
        });
  });

  test('ShowsChargingIconWhenAdapterConnected', () => {
    const expectedBatteryIcon =
        getDiagnosticsIcon(BATTERY_ICON_PREFIX + 'charging');
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          assertEquals(
              chromeos.diagnostics.mojom.ExternalPowerSource.kAc,
              getPowerAdapterStatus());
          assertEquals(expectedBatteryIcon, batteryStatusElement.batteryIcon);
        });
  });

  test('ShowsCorrectIconForBatteryPercentage', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus2, fakeBatteryHealth2)
        .then(() => {
          assertEquals(
              getPowerAdapterStatus(),
              chromeos.diagnostics.mojom.ExternalPowerSource.kDisconnected);

          const expectedIconRange = '71-77';
          assertEquals(
              getDiagnosticsIcon(BATTERY_ICON_PREFIX + expectedIconRange),
              batteryStatusElement.batteryIcon);
        });
  });
}
