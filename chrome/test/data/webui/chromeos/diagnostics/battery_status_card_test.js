// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/battery_status_card.js';

import {BatteryChargeStatus, BatteryHealth, BatteryInfo, SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {mojoString16ToString} from 'chrome://diagnostics/mojo_utils.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

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

  test('BatteryStatusCardPopulated', () => {
    return initializeBatteryStatusCard(
               fakeBatteryInfo, fakeBatteryChargeStatus, fakeBatteryHealth)
        .then(() => {
          const dataPoints =
              dx_utils.getDataPointElements(batteryStatusElement);
          assertEquals(
              fakeBatteryChargeStatus[0].currentNowMilliamps,
              dataPoints[0].value);
          assertEquals(
              fakeBatteryHealth[0].chargeFullDesignMilliampHours,
              dataPoints[1].value);
          assertEquals(
              fakeBatteryChargeStatus[0].chargeFullNowMilliampHours,
              dataPoints[2].value);
          assertEquals(
              fakeBatteryChargeStatus[0].chargeNowMilliampHours,
              dataPoints[3].value);
          assertEquals(
              mojoString16ToString(fakeBatteryChargeStatus[0].powerTime),
              dataPoints[4].value);
          assertEquals(
              fakeBatteryChargeStatus[0].powerAdapterStatus,
              dataPoints[5].value);
          assertEquals(fakeBatteryHealth[0].cycleCount, dataPoints[6].value);

          const barChart =
              dx_utils.getPercentBarChartElement(batteryStatusElement);
          assertEquals(
              fakeBatteryChargeStatus[0].chargeFullNowMilliampHours,
              barChart.max);
          assertEquals(
              fakeBatteryChargeStatus[0].chargeNowMilliampHours,
              barChart.value);
        });
  });
}
