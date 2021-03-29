// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, RoutineType, SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage, fakePowerRoutineResults, fakeRoutineResults, fakeSystemInfo, fakeSystemInfoWithoutBattery} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

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

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.instance_ = DiagnosticsBrowserProxy;
  });

  setup(() => {
    document.body.innerHTML = '';

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

    setSystemRoutineControllerForTesting(routineController);
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

  /**
   * Get the session log button.
   * @return {!CrButtonElement}
   */
  function getSessionLogButton() {
    return /** @type {!CrButtonElement} */ (page.$$('.session-log-button'));
  }

  /**
   * Clicks the session log button.
   * @return {!Promise}
   */
  function clickSessionLogButton() {
    getSessionLogButton().click();
    return flushTasks();
  }

  /**
   * Returns whether the toast is visible or not.
   * @return {boolean}
   */
  function isToastVisible() {
    return page.$$('cr-toast').open;
  }

  /**
   * @param {boolean} isLoggedIn
   * @suppress {visibility} // access private member
   * @return {!Promise}
   */
  function changeLoggedInState(isLoggedIn) {
    page.isLoggedIn_ = isLoggedIn;
    return flushTasks();
  }

  /**
   * Get the caution banner.
   * @return {!HTMLElement}
   */
  function getCautionBanner() {
    return /** @type {!HTMLElement} */ (page.$$('#banner'));
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

  test('SaveSessionLogSuccessShowsToast', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(true);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.$$('#toast'),
                loadTimeData.getString('sessionLogToastTextSuccess'));
          });
        });
  });

  test('SaveSessionLogFailure', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(false);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.$$('#toast'),
                loadTimeData.getString('sessionLogToastTextFailure'));
          });
        });
  });

  test('SessionLogHiddenWhenNotLoggedIn', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (false)))
        .then(() => assertFalse(isVisible(getSessionLogButton())));
  });

  test('SessionLogShownWhenLoggedIn', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (true)))
        .then(() => assertTrue(isVisible(getSessionLogButton())));
  });

  test('RunningCpuTestsShowsBanner', () => {
    /** @type {?RoutineSectionElement} */
    let routineSection;
    /** @type {!Array<!RoutineType>} */
    const routines = [
      chromeos.diagnostics.mojom.RoutineType.kCpuCache,
    ];
    routineController.setFakeStandardRoutineResult(
        chromeos.diagnostics.mojom.RoutineType.kCpuCache,
        chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed);
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          routineSection = dx_utils.getRoutineSection(page.$$('cpu-card'));
          routineSection.routines = routines;
          assertFalse(isVisible(getCautionBanner()));
          return flushTasks();
        })
        .then(() => {
          dx_utils.getRunTestsButtonFromSection(routineSection).click();
          return flushTasks();
        })
        .then(() => {
          assertTrue(isVisible(getCautionBanner()));
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => assertFalse(isVisible(getCautionBanner())));
  });
}
