// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/battery_status_card.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {BatteryStatusCardElement} from 'chrome://diagnostics/battery_status_card.js';
import {getDiagnosticsIcon} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakeBatteryChargeStatus, fakeBatteryChargeStatus2, fakeBatteryChargeStatus3, fakeBatteryHealth, fakeBatteryHealth2, fakeBatteryHealth3, fakeBatteryInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, ExternalPowerSource} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {RoutineType} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

const BATTERY_ICON_PREFIX = 'battery-';

suite('batteryStatusCardTestSuite', function() {
  let batteryStatusElement: BatteryStatusCardElement|null = null;

  const provider: FakeSystemDataProvider = new FakeSystemDataProvider();

  suiteSetup(() => {
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    batteryStatusElement?.remove();
    batteryStatusElement = null;
    provider?.reset();
  });

  function initializeBatteryStatusCard(
      batteryInfo: BatteryInfo, batteryChargeStatus: BatteryChargeStatus[],
      batteryHealth: BatteryHealth[]): Promise<void> {
    assertFalse(!!batteryStatusElement);

    // Initialize the fake data.
    provider.setFakeBatteryChargeStatus(batteryChargeStatus);
    provider.setFakeBatteryHealth(batteryHealth);
    provider.setFakeBatteryInfo(batteryInfo);

    // Add the battery status card to the DOM.
    batteryStatusElement = document.createElement('battery-status-card');
    assert(batteryStatusElement);
    document.body.appendChild(batteryStatusElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   */
  function getRoutineSection(): RoutineSectionElement {
    assert(batteryStatusElement);
    return strictQuery(
        'routine-section', batteryStatusElement.shadowRoot,
        RoutineSectionElement);
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   */
  function getRunTestsButton(): CrButtonElement {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assert(button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   */
  function isRunTestsButtonDisabled(): boolean {
    return getRunTestsButton().disabled;
  }


  function getPowerAdapterStatus(): ExternalPowerSource {
    assert(batteryStatusElement);
    return batteryStatusElement.getBatteryChargeStatusForTesting()
        .powerAdapterStatus;
  }

  test('BatteryStatusCardPopulated', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          assert(batteryStatusElement);
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(
                  batteryStatusElement, '#batteryHealth'),
              `${fakeBatteryHealth[0]!.batteryWearPercentage}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#batteryHealth')
                  .tooltipText,
              loadTimeData.getString('batteryHealthTooltipText'));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(batteryStatusElement, '#cycleCount'),
              `${fakeBatteryHealth[0]!.cycleCount}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#cycleCount')
                  .tooltipText,
              loadTimeData.getString('cycleCountTooltipText'));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(batteryStatusElement, '#currentNow'),
              `${fakeBatteryChargeStatus[0]!.currentNowMilliamps}`);
          dx_utils.assertTextContains(
              dx_utils.getDataPoint(batteryStatusElement, '#currentNow')
                  .tooltipText,
              loadTimeData.getString('currentNowTooltipText'));
          dx_utils.assertElementContainsText(
              batteryStatusElement!.shadowRoot!.querySelector(
                  '#batteryStatusChipInfo'),
              `${fakeBatteryHealth[0]!.chargeFullDesignMilliampHours}`);
          const barChart =
              dx_utils.getPercentBarChartElement(batteryStatusElement);
          assertEquals(
              fakeBatteryHealth[0]!.chargeFullNowMilliampHours, barChart.max);
          assertEquals(
              fakeBatteryChargeStatus[0]!.chargeNowMilliampHours,
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

          batteryStatusElement!.onBatteryChargeStatusUpdated(
              (fakeBatteryChargeStatus[2] as BatteryChargeStatus));
          return flushTasks();
        })
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(routineSectionElement.routines.length, 1);
          assertEquals(
              routineSectionElement.routines[0], RoutineType.kBatteryDischarge);

          batteryStatusElement!.onBatteryChargeStatusUpdated(
              (fakeBatteryChargeStatus[3] as BatteryChargeStatus));
          return flushTasks();
        })
        .then(() => {
          const routineSectionElement = getRoutineSection();

          assertEquals(
              routineSectionElement.additionalMessage,
              loadTimeData.getString('batteryChargeTestFullMessage'));
          assertTrue(isRunTestsButtonDisabled());
          assertTrue(isVisible(
              routineSectionElement.shadowRoot!.querySelector('#messageIcon')));
        });
  });

  test('ShowsChargingIconWhenAdapterConnected', () => {
    const expectedBatteryIcon =
        getDiagnosticsIcon(BATTERY_ICON_PREFIX + 'charging');
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          assertEquals(ExternalPowerSource.kAc, getPowerAdapterStatus());
          assertEquals(expectedBatteryIcon, batteryStatusElement!.batteryIcon);
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
              batteryStatusElement!.batteryIcon);
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
              batteryStatusElement!.batteryIcon);
        });
  });
});
