// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';

import {Network, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePowerRoutineResults, fakeRoutineResults, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function connectivityCardTestSuite() {
  /** @type {?ConnectivityCardElement} */
  let connectivityCardElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {!Array<!RoutineType>} */
  const defaultRoutineOverride = [RoutineType.kCaptivePortal];

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);

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

  setup(() => {
    document.body.innerHTML = '';
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
    return connectivityCardElement.routines_;
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
    routineSection.runTestsAutomatically = true;
  }

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.$$('#cardTitle'), 'Ethernet (Online)');
    });
  });

  test('CardConnectionChipInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.$$('#defaultConnectionChip'),
          connectivityCardElement.i18n('networkDefaultConnectionLabel'));
    });
  });

  test(
      'ConnectivityCardPopulated', () => {
        return initializeConnectivityCard('ethernetGuid').then(() => {
          const ethernetInfoElement = dx_utils.getEthernetInfoElement(
              connectivityCardElement.$$('network-info'));
          const linkSpeedDataPoint =
              dx_utils.getDataPoint(ethernetInfoElement, '#linkSpeed');
          assertTrue(isVisible(linkSpeedDataPoint));
          assertEquals(linkSpeedDataPoint.header, 'Link Speed');
          // TODO(ashleydp): Update expectation when link speed data added.
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(ethernetInfoElement, '#linkSpeed'),
              '');
        });
      });

  test('TestsRunAutomaticallyWhenPageIsActive', () => {
    return initializeConnectivityCard('ethernetGuid', true)
        .then(() => assertTrue(connectivityCardElement.isTestRunning));
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
}
