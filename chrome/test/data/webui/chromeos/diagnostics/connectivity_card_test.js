// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';

import {Network, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePowerRoutineResults, fakeRoutineResults, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function connectivityCardTestSuite() {
  /** @type {?ConnectivityCardElement} */
  let connectivityCardElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {!Array<!RoutineType>} */
  const defaultRoutineOverride = [new RoutineGroup(
      [RoutineType.kCaptivePortal], 'captivePortalRoutineText')];

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    /** @type {!Array<!RoutineType>} */
    const supportedRoutines =
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()];
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
    connectivityCardElement.remove();
    connectivityCardElement = null;
    provider.reset();
  });

  /**
   * Configures provider based on active guid.
   * @param {string} activeGuid
   */
  function configureProviderForGuid(activeGuid) {
    /**
     * @type {{cellularGuid: !Array<!Network>, ethernetGuid: !Array<!Network>,
     *     wifiGuid: !Array<!Network>}}
     */
    const networkStates = {
      cellularGuid: [fakeCellularNetwork],
      ethernetGuid: [fakeEthernetNetwork],
      wifiGuid: [fakeWifiNetwork],
    };
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState(activeGuid, networkStates[activeGuid]);
  }

  /**
   * @suppress {visibility} // access private member
   * return {!Array<!RoutineType>>}
   */
  function getRoutines() {
    assertTrue(!!connectivityCardElement);
    let routines = [];
    for (let routineGroup of connectivityCardElement.routineGroups_) {
      routines = [...routines, ...routineGroup.routines];
    }

    return routines;
  }

  /**
   * @param {string} activeGuid
   * @param {boolean=} isActive
   */
  function initializeConnectivityCard(activeGuid, isActive = false) {
    assertFalse(!!connectivityCardElement);
    configureProviderForGuid(activeGuid);
    // Add the connectivity card to the DOM.
    connectivityCardElement = /** @type {!ConnectivityCardElement} */ (
        document.createElement('connectivity-card'));
    assertTrue(!!connectivityCardElement);
    connectivityCardElement.activeGuid = activeGuid;
    connectivityCardElement.isActive = isActive;
    document.body.appendChild(connectivityCardElement);
    // Override routines in routine-section with reduced set.
    setRoutineSectionRoutines(defaultRoutineOverride);

    return flushTasks();
  }

  /**
   * Override routines in routine-section with provided set.
   * @param {!Array<!RoutineType>} routines
   */
  function setRoutineSectionRoutines(routines) {
    assertTrue(!!connectivityCardElement);
    const routineSection = dx_utils.getRoutineSection(connectivityCardElement);
    routineSection.routines = routines;
  }

  /**
   * @param {string} guid
   * @return {!Promise}
   */
  function changeActiveGuid(guid) {
    connectivityCardElement.activeGuid = guid;
    return flushTasks();
  }

  /**
   * @suppress {visibility} // access private method for testing.
   * @return {!Promise}
   */
  function stopTests() {
    connectivityCardElement.getRoutineSectionElem_().stopTests();
    return flushTasks();
  }

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.$$('#cardTitle'), 'Ethernet');
    });
  });

  test('CardMacAddressChipInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.$$('#macAddressChip'),
          'MAC: 81:C5:A6:30:3F:31');
    });
  });

  test('CardNetworkIconEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      assertTrue(isVisible(
          /** @type {!Element} */ (connectivityCardElement.$$('#icon'))));
    });
  });

  test('TestsRunAutomaticallyWhenPageIsActive', () => {
    return initializeConnectivityCard('ethernetGuid', true)
        .then(
            () => assertEquals(
                TestSuiteStatus.kRunning,
                connectivityCardElement.testSuiteStatus));
  });

  test(
      'CardIpConfigurationDrawerInitializedCorrectly', () => {
        return initializeConnectivityCard('ethernetGuid').then(() => {
          const ipConfigInfoDrawerElement =
              /** @type IpConfigInfoDrawerElement */ (
                  connectivityCardElement.$$('#ipConfigInfoDrawer'));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (ipConfigInfoDrawerElement)));
          assertDeepEquals(
              fakeEthernetNetwork, ipConfigInfoDrawerElement.network);
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
                TestSuiteStatus.kRunning,
                connectivityCardElement.testSuiteStatus))
        .then(() => stopTests())
        .then(
            () => assertEquals(
                TestSuiteStatus.kNotRunning,
                connectivityCardElement.testSuiteStatus))
        .then(() => changeActiveGuid('wifiGuid'))
        .then(
            () => assertEquals(
                TestSuiteStatus.kRunning,
                connectivityCardElement.testSuiteStatus));
  });
}
