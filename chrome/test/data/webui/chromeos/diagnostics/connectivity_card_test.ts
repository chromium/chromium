// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ConnectivityCardElement} from 'chrome://diagnostics/connectivity_card.js';
import {DiagnosticsNetworkIconElement} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {CellularNetwork, EthernetNetwork, WiFiNetwork} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('connectivityCardTestSuite', function() {
  let connectivityCardElement: ConnectivityCardElement|null = null;

  const provider = new FakeNetworkHealthProvider();

  let routineController: FakeSystemRoutineController;

  const defaultRoutineOverride: RoutineGroup = new RoutineGroup(
      [{
        routine: RoutineType.kCaptivePortal,
        blocking: false,
      }],
      'captivePortalRoutineText');

  suiteSetup(() => {
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    const supportedRoutines: RoutineType[] = routineController.getAllRoutines();
    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(supportedRoutines);
    // Configure default routine results. Results can also be set in individual
    // tests as needed.
    supportedRoutines.forEach(
        routine => routineController.setFakeStandardRoutineResult(
            routine, StandardRoutineResult.kTestPassed));

    setSystemRoutineControllerForTesting(routineController);
  });

  teardown(() => {
    connectivityCardElement?.remove();
    connectivityCardElement = null;
    provider.reset();
  });

  interface MyIndexedObject {
    cellularGuid: CellularNetwork[];
    ethernetGuid: EthernetNetwork[];
    wifiGuid: WiFiNetwork[];
  }
  /**
   * Configures provider based on active guid.
   */
  function configureProviderForGuid(activeGuid: string): void {
    const networkStates: MyIndexedObject = {
      'cellularGuid': [fakeCellularNetwork],
      'ethernetGuid': [fakeEthernetNetwork],
      'wifiGuid': [fakeWifiNetwork],
    };
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState(
        activeGuid, networkStates[(activeGuid as keyof MyIndexedObject)]);
  }

  function getRoutines(): RoutineType[] {
    assert(connectivityCardElement);
    let routines: RoutineType[] = [];
    for (const routineGroup of
             connectivityCardElement.getRoutineGroupsForTesting()) {
      routines = [...routines, ...routineGroup.routines];
    }

    return routines;
  }

  function initializeConnectivityCard(
      activeGuid: string, isActive = false): Promise<void> {
    assertFalse(!!connectivityCardElement);
    configureProviderForGuid(activeGuid);
    // Add the connectivity card to the DOM.
    connectivityCardElement = document.createElement('connectivity-card');
    assert(connectivityCardElement);
    connectivityCardElement.activeGuid = activeGuid;
    connectivityCardElement.isActive = isActive;
    document.body.appendChild(connectivityCardElement);
    // Override routines in routine-section with reduced set.
    setRoutineSectionRoutines(defaultRoutineOverride.routines);

    return flushTasks();
  }

  /**
   * Override routines in routine-section with provided set.
   */
  function setRoutineSectionRoutines(routines: RoutineType[]): void {
    assert(connectivityCardElement);
    const routineSection = dx_utils.getRoutineSection(connectivityCardElement);
    routineSection.routines = routines;
  }

  function changeActiveGuid(guid: string): Promise<void> {
    assert(connectivityCardElement);
    connectivityCardElement.activeGuid = guid;
    return flushTasks();
  }

  function stopTests(): Promise<void> {
    assert(connectivityCardElement);
    connectivityCardElement.getRoutineSectionElemForTesting().stopTests();
    return flushTasks();
  }

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement!.shadowRoot!.querySelector('#cardTitle'),
          'Ethernet');
    });
  });

  test('CardMacAddressChipInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement!.shadowRoot!.querySelector('#macAddressChip'),
          'MAC: 81:C5:A6:30:3F:31');
    });
  });

  test('CardNetworkIconEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      assertTrue(isVisible(
        strictQuery('#icon', connectivityCardElement!.shadowRoot, DiagnosticsNetworkIconElement)));
    });
  });

  test('TestsRunAutomaticallyWhenPageIsActive', () => {
    return initializeConnectivityCard('ethernetGuid', true)
        .then(
            () => assertEquals(
                TestSuiteStatus.RUNNING,
                connectivityCardElement!.testSuiteStatus));
  });

  test('CardIpConfigurationDrawerInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      const ipConfigInfoDrawerElement = strictQuery(
          '#ipConfigInfoDrawer', connectivityCardElement!.shadowRoot,
          IpConfigInfoDrawerElement);
      assertTrue(isVisible(ipConfigInfoDrawerElement));
      assertDeepEquals(fakeEthernetNetwork, ipConfigInfoDrawerElement.network);
    });
  });

  test('RoutinesForWiFiIncludedWhenNetworkIsWifi', () => {
    return initializeConnectivityCard('wifiGuid').then(() => {
      const routines = getRoutines();
      assertTrue(routines.includes(RoutineType.kSignalStrength));
      assertTrue(routines.includes(RoutineType.kHasSecureWiFiConnection));
    });
  });

  test('RoutinesForWiFiExcludedWhenNetworkIsNotWifi', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      const routines = getRoutines();
      assertFalse(routines.includes(RoutineType.kSignalStrength));
      assertFalse(routines.includes(RoutineType.kHasSecureWiFiConnection));
    });
  });

  test('TestsRestartWhenGuidChanges', () => {
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    return initializeConnectivityCard('ethernetGuid', true)
        .then(
            () => assertEquals(
                TestSuiteStatus.RUNNING,
                connectivityCardElement!.testSuiteStatus))
        .then(() => stopTests())
        .then(
            () => assertEquals(
                TestSuiteStatus.NOT_RUNNING,
                connectivityCardElement!.testSuiteStatus))
        .then(() => changeActiveGuid('wifiGuid'))
        .then(
            () => assertEquals(
                TestSuiteStatus.RUNNING,
                connectivityCardElement!.testSuiteStatus));
  });
});
