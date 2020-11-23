// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';

import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage, fakeSystemInfo, fakeSystemInfoWithoutBattery} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

/**
 * @param {Array<?T>} cards
 * @template T
 * @throws {!Error}
 */
function assertRunTestButtonsDisabled(cards) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertTrue(runTestsButton.disabled);
  });
}

/**
 * @param {Array<?T>} cards
 * @template T
 * @throws {!Error}
 */
function assertRunTestButtonsEnabled(cards) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertFalse(runTestsButton.disabled);
  });
}

export function appTestSuite() {
  /** @type {?DiagnosticsAppElement} */
  let page = null;

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
    if (page) {
      page.remove();
    }
    page = null;
    provider.reset();
  });

  /**
   *
   * @param {!SystemInfo} systemInfo
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatus
   * @param {!Array<!BatteryHealth>} batteryHealth
   * @param {!BatteryInfo} batteryInfo
   * @param {!Array<!CpuUsage>} cpuUsage
   * @param {!Array<!MemoryUsage>} memoryUsage
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

    page = /** @type {!DiagnosticsAppElement} */ (
        document.createElement('diagnostics-app'));
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

  test('AllRunTestsButtonsDisabledWhileRunning', () => {
    let cards = null;
    let memoryRoutinesSection = null;
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          const batteryStatusCard = page.$$('battery-status-card');
          const cpuCard = page.$$('cpu-card');
          const memoryCard = page.$$('memory-card');
          cards = [batteryStatusCard, cpuCard, memoryCard];

          memoryRoutinesSection = dx_utils.getRoutineSection(memoryCard);
          memoryRoutinesSection.isTestRunning = true;
          return flushTasks();
        })
        .then(() => {
          assertRunTestButtonsDisabled(cards);
          memoryRoutinesSection.isTestRunning = false;
          return flushTasks();
        })
        .then(() => assertRunTestButtonsEnabled(cards));
  });
}
