// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/battery_status_card.js';

import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flushTasks} from 'chrome://test/test_util.m.js';
import * as dx_utils from './diagnostics_test_utils.js';

suite('BatteryStatusCardTest', () => {
  /** @type {?HTMLElement} */
  let batteryStatusElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
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
   * @param {!BatteryChargeStatus} batteryChargeStatus
   * @param {!BatteryHealth} batteryHealth
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
    batteryStatusElement = document.createElement('battery-status-card');
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
              fakeBatteryChargeStatus[0].current_now_milliamps,
              dataPoints[0].value);
          assertEquals(
              fakeBatteryHealth[0].charge_full_design_milliamp_hours,
              dataPoints[1].value);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_full_now_milliamp_hours,
              dataPoints[2].value);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_now_milliamp_hours,
              dataPoints[3].value);
          assertEquals(
              fakeBatteryChargeStatus[0].power_time, dataPoints[4].value);
          assertEquals(
              fakeBatteryChargeStatus[0].power_adapter_status,
              dataPoints[5].value);
          assertEquals(fakeBatteryHealth[0].cycle_count, dataPoints[6].value);

          const barChart =
              dx_utils.getPercentBarChartElement(batteryStatusElement);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_full_now_milliamp_hours,
              barChart.max);
          assertEquals(
              fakeBatteryChargeStatus[0].charge_now_milliamp_hours,
              barChart.value);
        });
  });
});