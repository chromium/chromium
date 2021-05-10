// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';

import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkInfoTestSuite() {
  /** @type {?NetworkInfoElement} */
  let networkInfoElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkInfoElement.remove();
    networkInfoElement = null;
    provider.reset();
  });

  /**
   * @param {string} guid
   */
  function initializeNetworkInfo(guid) {
    assertFalse(!!networkInfoElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);

    // Add the network info to the DOM.
    networkInfoElement = /** @type {!NetworkInfoElement} */ (
        document.createElement('network-info'));
    assertTrue(!!networkInfoElement);
    networkInfoElement.guid = guid;
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  /**
   * @param {string} guid
   * @return {!Promise}
   */
  function changeGuid(guid) {
    networkInfoElement.guid = guid;
    return flushTasks();
  }

  test('CorrectInfoElementShown', () => {
    return initializeNetworkInfo('wifiGuid')
        .then(() => {
          // wifi-info should be visible.
          assertTrue(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
          return changeGuid('cellularGuid');
        })
        .then(() => {
          // cellular-info should be visible.
          assertTrue(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          return changeGuid('ethernetGuid');
        })
        .then(() => {
          // ethernet-info should be visible.
          assertTrue(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
        });
  });
}