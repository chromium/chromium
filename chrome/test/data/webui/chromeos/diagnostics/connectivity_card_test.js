// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';

import {Network, RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePowerRoutineResults, fakeRoutineResults} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function connectivityCardTestSuite() {
  /** @type {?ConnectivityCardElement} */
  let connectivityCardElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

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
   * @param {string} activeGuid
   * @param {!Array<!Network>} networkStateList
   * @param {boolean=} isActive
   */
  function initializeConnectivityCard(
      activeGuid, networkStateList, isActive = false) {
    assertFalse(!!connectivityCardElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState(activeGuid, networkStateList);

    // Add the connectivity card to the DOM.
    connectivityCardElement = /** @type {!ConnectivityCardElement} */ (
        document.createElement('connectivity-card'));
    assertTrue(!!connectivityCardElement);
    connectivityCardElement.activeGuid = activeGuid;
    connectivityCardElement.isActive = isActive;
    document.body.appendChild(connectivityCardElement);

    /** @type {!Array<!RoutineType>} */
    const routines = [RoutineType.kCpuCache];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    const routineSection = dx_utils.getRoutineSection(connectivityCardElement);
    routineSection.routines = routines;
    routineSection.runTestsAutomatically = true;
    return flushTasks();
  }

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeConnectivityCard('ethernetGuid', [fakeEthernetNetwork])
        .then(() => {
          dx_utils.assertElementContainsText(
              connectivityCardElement.$$('#cardTitle'), 'Ethernet (Online)');
        });
  });

  test('ConnectivityCardPopulated', () => {
    return initializeConnectivityCard('ethernetGuid', [fakeEthernetNetwork])
        .then(() => {
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
    return initializeConnectivityCard(
               'ethernetGuid', [fakeEthernetNetwork], true)
        .then(() => assertTrue(connectivityCardElement.isTestRunning));
  });
}
