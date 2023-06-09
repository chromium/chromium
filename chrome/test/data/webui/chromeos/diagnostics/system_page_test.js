// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://diagnostics/system_page.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakeMemoryUsageHighAvailableMemory, fakeNetworkGuidInfoList, fakeSystemInfo, fakeSystemInfoWithoutBattery, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {SystemPageElement} from 'chrome://diagnostics/system_page.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

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

suite('systemPageTestSuite', function() {
  /** @type {?SystemPageElement} */
  let page = null;

  /** @type {?FakeSystemDataProvider} */
  let systemDataProvider = null;

  /** @type {?FakeNetworkHealthProvider} */
  let networkHealthProvider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    systemDataProvider = new FakeSystemDataProvider();
    networkHealthProvider = new FakeNetworkHealthProvider();

    setSystemDataProviderForTesting(systemDataProvider);
    setNetworkHealthProviderForTesting(networkHealthProvider);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    page.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  /**
   * @param {!SystemInfo} systemInfo
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatus
   * @param {!Array<!BatteryHealth>} batteryHealth
   * @param {!BatteryInfo} batteryInfo
   * @param {!Array<!CpuUsage>} cpuUsage
   * @param {!Array<!MemoryUsage>} memoryUsage
   */
  function initializeSystemPage(
      systemInfo, batteryChargeStatus, batteryHealth, batteryInfo, cpuUsage,
      memoryUsage) {
    assertFalse(!!page);

    // Initialize the fake data.
    systemDataProvider.setFakeSystemInfo(systemInfo);
    systemDataProvider.setFakeBatteryChargeStatus(batteryChargeStatus);
    systemDataProvider.setFakeBatteryHealth(batteryHealth);
    systemDataProvider.setFakeBatteryInfo(batteryInfo);
    systemDataProvider.setFakeCpuUsage(cpuUsage);
    systemDataProvider.setFakeMemoryUsage(memoryUsage);

    networkHealthProvider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    networkHealthProvider.setFakeNetworkState(
        'ethernetGuid', [fakeEthernetNetwork]);
    networkHealthProvider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    networkHealthProvider.setFakeNetworkState(
        'cellularGuid', [fakeCellularNetwork]);

    page =
        /** @type {!SystemPageElement} */ (
            document.createElement('system-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * Get the caution banner.
   * @return {!HTMLElement}
   */
  function getCautionBanner() {
    return /** @type {!HTMLElement} */ (
        page.shadowRoot.querySelector('#banner'));
  }

  test('LandingPageLoaded', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          // Verify the overview card is in the page.
          const overview = page.shadowRoot.querySelector('#overviewCard');
          assertTrue(!!overview);

          // Verify the memory card is in the page.
          const memory = page.shadowRoot.querySelector('#memoryCard');
          assertTrue(!!memory);

          // Verify the CPU card is in the page.
          const cpu = page.shadowRoot.querySelector('#cpuCard');
          assertTrue(!!cpu);

          // Verify the battery status card is in the page.
          const batteryStatus =
              page.shadowRoot.querySelector('#batteryStatusCard');
          assertTrue(!!batteryStatus);
        });
  });

  test('BatteryStatusCardHiddenIfNotSupported', () => {
    return initializeSystemPage(
               fakeSystemInfoWithoutBattery, fakeBatteryChargeStatus,
               fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsage)
        .then(() => {
          // Verify the battery status card is not in the page.
          const batteryStatus =
              page.shadowRoot.querySelector('#batteryStatusCard');
          assertFalse(!!batteryStatus);
        });
  });

  test('AllRunTestsButtonsDisabledWhileRunning', () => {
    let cards = null;
    let memoryRoutinesSection = null;
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsageHighAvailableMemory)
        .then(() => {
          const batteryStatusCard =
              page.shadowRoot.querySelector('battery-status-card');
          const cpuCard = page.shadowRoot.querySelector('cpu-card');
          const memoryCard = page.shadowRoot.querySelector('memory-card');
          cards = [batteryStatusCard, cpuCard, memoryCard];
          assertRunTestButtonsEnabled(cards);

          memoryRoutinesSection = dx_utils.getRoutineSection(memoryCard);
          memoryRoutinesSection.testSuiteStatus = TestSuiteStatus.RUNNING;
          return flushTasks();
        })
        .then(() => {
          assertEquals(TestSuiteStatus.RUNNING, page.testSuiteStatus);
          assertRunTestButtonsDisabled(cards);
          memoryRoutinesSection.testSuiteStatus = TestSuiteStatus.NOT_RUNNING;
          return flushTasks();
        })
        .then(() => assertRunTestButtonsEnabled(cards));
  });

  test('RecordNavigationCalled', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          page.onNavigationPageChanged({isActive: false});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              0, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));

          DiagnosticsBrowserProxy.setPreviousView(NavigationView.CONNECTIVITY);
          page.onNavigationPageChanged({isActive: true});

          return flushTasks();
        })
        .then(() => {
          assertEquals(
              1, DiagnosticsBrowserProxy.getCallCount('recordNavigation'));
          assertArrayEquals(
              [NavigationView.CONNECTIVITY, NavigationView.SYSTEM],
              /** @type {!Array<!NavigationView>} */
              (DiagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
        });
  });
});
