// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://diagnostics/system_page.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

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
import {assert} from 'chrome://resources/js/assert.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

function assertRunTestButtonsDisabled(cards: any[]) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertTrue(runTestsButton.disabled);
  });
}

function assertRunTestButtonsEnabled(cards: any[]) {
  cards.forEach((card) => {
    const routineSection = dx_utils.getRoutineSection(card);
    const runTestsButton =
        dx_utils.getRunTestsButtonFromSection(routineSection);
    assertFalse(runTestsButton.disabled);
  });
}

suite('systemPageTestSuite', function() {
  let page: SystemPageElement|null = null;

  const systemDataProvider = new FakeSystemDataProvider();

  const networkHealthProvider = new FakeNetworkHealthProvider();

  const routineController = new FakeSystemRoutineController();

  const DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();

  suiteSetup(() => {
    setSystemDataProviderForTesting(systemDataProvider);
    setNetworkHealthProviderForTesting(networkHealthProvider);

    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);

    // Setup a fake routine controller.
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    page?.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  function initializeSystemPage(
      systemInfo: SystemInfo, batteryChargeStatus: BatteryChargeStatus[],
      batteryHealth: BatteryHealth[], batteryInfo: BatteryInfo,
      cpuUsage: CpuUsage[], memoryUsage: MemoryUsage[]): Promise<void> {
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

    page = document.createElement(SystemPageElement.is);
    assert(page);
    document.body.appendChild(page);
    return flushTasks();
  }

  test('LandingPageLoaded', () => {
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          assert(page);
          // Verify the overview card is in the page.
          const overview = page.shadowRoot!.querySelector('#overviewCard');
          assert(overview);

          // Verify the memory card is in the page.
          const memory = page.shadowRoot!.querySelector('#memoryCard');
          assert(memory);

          // Verify the CPU card is in the page.
          const cpu = page.shadowRoot!.querySelector('#cpuCard');
          assert(cpu);

          // Verify the battery status card is in the page.
          const batteryStatus =
              page.shadowRoot!.querySelector('#batteryStatusCard');
          assert(batteryStatus);
        });
  });

  test('BatteryStatusCardHiddenIfNotSupported', () => {
    return initializeSystemPage(
               fakeSystemInfoWithoutBattery, fakeBatteryChargeStatus,
               fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsage)
        .then(() => {
          // Verify the battery status card is not in the page.
          assert(page);
          const batteryStatus =
              page.shadowRoot!.querySelector('#batteryStatusCard');
          assertFalse(!!batteryStatus);
        });
  });

  test('AllRunTestsButtonsDisabledWhileRunning', () => {
    let cards: Element[];
    let memoryRoutinesSection: RoutineSectionElement|null = null;
    return initializeSystemPage(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage,
               fakeMemoryUsageHighAvailableMemory)
        .then(() => {
          assert(page);
          const batteryStatusCard =
              page.shadowRoot!.querySelector('battery-status-card');
          const cpuCard = page.shadowRoot!.querySelector('cpu-card');
          const memoryCard = page.shadowRoot!.querySelector('memory-card');
          cards = [batteryStatusCard, cpuCard, memoryCard] as Element[];
          assertRunTestButtonsEnabled(cards);

          memoryRoutinesSection = dx_utils.getRoutineSection(memoryCard);
          assert(memoryRoutinesSection);
          memoryRoutinesSection.testSuiteStatus = TestSuiteStatus.RUNNING;
          return flushTasks();
        })
        .then(() => {
          assert(page);
          assertEquals(TestSuiteStatus.RUNNING, page.testSuiteStatus);
          assertRunTestButtonsDisabled(cards);
          assert(memoryRoutinesSection);
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
          assert(page);
          page.onNavigationPageChanged({isActive: false});

          return flushTasks();
        })
        .then(() => {
          assert(page);
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
              (DiagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
        });
  });
});
