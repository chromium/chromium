// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_card.js';

import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkCardTestSuite() {
  /** @type {?NetworkCardElement} */
  let networkCardElement = null;

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
    networkCardElement.remove();
    networkCardElement = null;
    provider.reset();
  });

  /**
   * @param {string} guid
   */
  function initializeNetworkCard(guid) {
    assertFalse(!!networkCardElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);

    // Add the network info to the DOM.
    networkCardElement = /** @type {!NetworkCardElement} */ (
        document.createElement('network-card'));
    assertTrue(!!networkCardElement);
    networkCardElement.guid = guid;
    document.body.appendChild(networkCardElement);

    return flushTasks();
  }

  test('CardTitleWiFiConnectedInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'WiFi (Connected)');
    });
  });

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeNetworkCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Ethernet (Online)');
    });
  });

  test('EthernetNotDetectedStateVisible', () => {
    return initializeNetworkCard('ethernetGuid')
        .then(
            () => assertTrue(isVisible(
                /** @type {!HTMLElement} */ (
                    networkCardElement.$$('#troubleConnectingContainer')))));
  });

  test('EthernetNotDetectedStateHidden', () => {
    // Trouble connecting state should only be visible for Ethernet networks.
    return initializeNetworkCard('wifiGuid')
        .then(
            () => assertFalse(isVisible(
                /** @type {!HTMLElement} */ (
                    networkCardElement.$$('#troubleConnectingContainer')))));
  });
}