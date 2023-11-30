// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/battery_status_card.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {BatteryStatusCardElement} from 'chrome://diagnostics/battery_status_card.js';
import {getDiagnosticsIcon} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeBatteryChargeStatus, fakeBatteryChargeStatus2, fakeBatteryChargeStatus3, fakeBatteryHealth, fakeBatteryHealth2, fakeBatteryHealth3, fakeBatteryInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, ExternalPowerSource} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {RoutineType} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {TextBadgeElement} from 'chrome://diagnostics/text_badge.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isChildVisible, isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

const BATTERY_ICON_PREFIX = 'battery-';

suite('batteryStatusCardTestSuite', function() {
  /** @type {?BatteryStatusCardElement} */
  let batteryStatusElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
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
        batteryStatusElement.shadowRoot.querySelector('routine-section'));
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
        routineSectionElement.shadowRoot.querySelector('#testStatusBadge'));
  }

  /**
   * Returns the status text.
   * @return {!HTMLElement}
   */
  function getStatusTextElement() {
    const routineSectionElement = getRoutineSection();

    const statusText =
        /** @type {!HTMLElement} */ (
            routineSectionElement.shadowRoot.querySelector('#testStatusText'));
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
   * Get batteryChargeStatus.powerAdapterStatus private member for testing.
   * @suppress {visibility} // access private member
   * @return {!ExternalPowerSource}
   */
  function getPowerAdapterStatus() {
    assertTrue(!!batteryStatusElement);
    return batteryStatusElement.batteryChargeStatus.powerAdapterStatus;
  }

  test('BatteryStatusCardPopulated', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(
                  batteryStatusElement, '#batteryHealth'),
              `${fakeBatteryHealth[0].batteryWearPercentage}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#batteryHealth')
                  .tooltipText,
              loadTimeData.getString('batteryHealthTooltipText'));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(batteryStatusElement, '#cycleCount'),
              `${fakeBatteryHealth[0].cycleCount}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#cycleCount')
                  .tooltipText,
              loadTimeData.getString('cycleCountTooltipText'));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(batteryStatusElement, '#currentNow'),
              `${fakeBatteryChargeStatus[0].currentNowMilliamps}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#currentNow')
                  .tooltipText,
              loadTimeData.getString('currentNowTooltipText'));
          dx_utils.assertElementContainsText(
              batteryStatusElement.shadowRoot.querySelector(
                  '#batteryStatusChipInfo'),
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
              routineSectionElement.routines[0], RoutineType.kBatteryCharge);

          batteryStatusElement.onBatteryChargeStatusUpdated(
              fakeBatteryChargeStatus[2]);
          return flushTasks();
        })
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(routineSectionElement.routines.length, 1);
          assertEquals(
              routineSectionElement.routines[0], RoutineType.kBatteryDischarge);

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
              routineSectionElement.shadowRoot.querySelector('#messageIcon'))));
        });
  });

  test('ShowsChargingIconWhenAdapterConnected', () => {
    const expectedBatteryIcon =
        getDiagnosticsIcon(BATTERY_ICON_PREFIX + 'charging');
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          assertEquals(ExternalPowerSource.kAc, getPowerAdapterStatus());
          assertEquals(expectedBatteryIcon, batteryStatusElement.batteryIcon);
        });
  });

  test('ShowsCorrectIconForBatteryPercentage', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus2, fakeBatteryHealth2)
        .then(() => {
          assertEquals(
              getPowerAdapterStatus(), ExternalPowerSource.kDisconnected);

          const expectedIconRange = '71-77';
          assertEquals(
              getDiagnosticsIcon(BATTERY_ICON_PREFIX + expectedIconRange),
              batteryStatusElement.batteryIcon);
        });
  });

  test('ShowsCorrectIconForZeroBattery', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus3, fakeBatteryHealth3)
        .then(() => {
          assertEquals(
              getPowerAdapterStatus(), ExternalPowerSource.kDisconnected);

          const expectedIconRange = 'outline';
          assertEquals(
              getDiagnosticsIcon(BATTERY_ICON_PREFIX + expectedIconRange),
              batteryStatusElement.batteryIcon);
        });
  });
});
