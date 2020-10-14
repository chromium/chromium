// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage, fakeSystemInfo, fakeSystemInfoWithoutBattery} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('DiagnosticsAppTest', () => {
  /** @type {?DiagnosticsApp} */
  let page = null;

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
    if (page) {
      page.remove();
    }
    page = null;
    provider.reset();
  });

  /**
   *
   * @param {!SystemInfo} systemInfo
   * @param {!BatteryChargeStatus} batteryChargeStatus
   * @param {!BatteryHealth} batteryHealth
   * @param {!BatteryInfo} batteryInfo
   * @param {!CpuUsage} cpuUsage
   * @param {!MemoryUsage} memoryUsage
   */
  function initializeDiagnosticsApp(
      systemInfo, batteryChargeStatus, batteryHealth, batteryInfo, cpuUsage,
      memoryUsage) {
    assertFalse(!!page);

    // Initialize the fake data.
    provider.setFakeSystemInfo(systemInfo);
    provider.setFakeBatteryChargeStatus(batteryChargeStatus);
    provider.setFakeBatteryHealth(batteryHealth);
    provider.setFakeBatteryInfo(batteryInfo);
    provider.setFakeCpuUsage(cpuUsage);
    provider.setFakeMemoryUsage(memoryUsage);

    page = document.createElement('diagnostics-app');
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }
  test('LandingPageLoaded', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          // Verify the overview card is in the page.
          const overview = page.$$('#overviewCard');
          assertTrue(!!overview);

          // Verify the memory card is in the page.
          const memory = page.$$('#memoryCard');
          assertTrue(!!memory);

          // Verify the CPU card is in the page.
          const cpu = page.$$('#cpuCard');
          assertTrue(!!cpu);

          // Verify the battery status card is in the page.
          const batteryStatus = page.$$('#batteryStatusCard');
          assertTrue(!!batteryStatus);

          // Verify the session log button is in the page.
          const sessionLog = page.$$('.session-log-button');
          assertTrue(!!sessionLog);
        });
  });

  test('BatteryStatusCardHiddenIfNotSupported', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfoWithoutBattery, fakeBatteryChargeStatus,
               fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsage)
        .then(() => {
          // Verify the battery status card is not in the page.
          const batteryStatus = page.$$('#batteryStatusCard');
          assertFalse(!!batteryStatus);
        });
  });
});
