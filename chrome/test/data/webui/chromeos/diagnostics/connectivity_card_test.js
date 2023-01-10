// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {ConnectivityCardElement} from 'chrome://diagnostics/connectivity_card.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('connectivityCardTestSuite', function() {
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
    const supportedRoutines = routineController.getAllRoutines();
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
    for (const routineGroup of connectivityCardElement.routineGroups) {
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
    connectivityCardElement.getRoutineSectionElem().stopTests();
    return flushTasks();
  }

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.shadowRoot.querySelector('#cardTitle'),
          'Ethernet');
    });
  });

  test('CardMacAddressChipInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          connectivityCardElement.shadowRoot.querySelector('#macAddressChip'),
          'MAC: 81:C5:A6:30:3F:31');
    });
  });

  test('CardNetworkIconEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid').then(() => {
      assertTrue(isVisible(
          /** @type {!Element} */ (
              connectivityCardElement.shadowRoot.querySelector('#icon'))));
    });
  });

  test('TestsRunAutomaticallyWhenPageIsActive', () => {
    return initializeConnectivityCard('ethernetGuid', true)
        .then(
            () => assertEquals(
                TestSuiteStatus.RUNNING,
                connectivityCardElement.testSuiteStatus));
  });

  test(
      'CardIpConfigurationDrawerInitializedCorrectly', () => {
        return initializeConnectivityCard('ethernetGuid').then(() => {
          const ipConfigInfoDrawerElement =
              /** @type {IpConfigInfoDrawerElement} */ (
                  connectivityCardElement.shadowRoot.querySelector(
                      '#ipConfigInfoDrawer'));
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
                TestSuiteStatus.RUNNING,
                connectivityCardElement.testSuiteStatus))
        .then(() => stopTests())
        .then(
            () => assertEquals(
                TestSuiteStatus.NOT_RUNNING,
                connectivityCardElement.testSuiteStatus))
        .then(() => changeActiveGuid('wifiGuid'))
        .then(
            () => assertEquals(
                TestSuiteStatus.RUNNING,
                connectivityCardElement.testSuiteStatus));
  });
});
