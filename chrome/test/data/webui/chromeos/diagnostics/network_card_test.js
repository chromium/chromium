// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_card.js';

import {fakeCellularNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeDisconnectedWifiNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled, fakeWifiNetworkInvalidNameServers, fakeWifiNetworkNoIpAddress} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

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
    provider.setFakeNetworkState('wifiGuidDisabled', [fakeWifiNetworkDisabled]);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    provider.setFakeNetworkState(
        'ethernetDisconnectedGuid', [fakeDisconnectedEthernetNetwork]);
    provider.setFakeNetworkState(
        'wifiDisconnectedGuid', [fakeDisconnectedWifiNetwork]);
    provider.setFakeNetworkState('wifiPortalGuid', [fakePortalWifiNetwork]);
    provider.setFakeNetworkState(
        'ethernetConnectingGuid', [fakeConnectingEthernetNetwork]);
    provider.setFakeNetworkState(
        'wifiGuidInvalidNameServers', [fakeWifiNetworkInvalidNameServers]);
    provider.setFakeNetworkState(
        'wifiGuidNoIpAddress', [fakeWifiNetworkNoIpAddress]);
    // Add the network info to the DOM.
    networkCardElement = /** @type {!NetworkCardElement} */ (
        document.createElement('network-card'));
    assertTrue(!!networkCardElement);
    networkCardElement.guid = guid;
    document.body.appendChild(networkCardElement);

    return flushTasks();
  }

  /**
   * @param {string} guid
   * @return {!Promise}
   */
  function changeGuid(guid) {
    networkCardElement.guid = guid;
    return flushTasks();
  }

  /**
   * @return {!HTMLElement}
   */
  function getTroubleConnectingElement() {
    return /** @type {!HTMLElement} */ (
        networkCardElement.$$('#networkTroubleshooting'));
  }

  /** @return {!Element} */
  function getIpConfigDrawerElement() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (
        networkCardElement.$$('#ipConfigInfoDrawer'));
  }

  /** @return {!Element} */
  function getNetworkInfoElement() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (networkCardElement.$$('network-info'));
  }

  /** @return {!Element} */
  function getNetworkIcon() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (networkCardElement.$$('#icon'));
  }

  /** @return {!Element} */
  function getEthernetInfoElement() {
    const networkInfoElement = getNetworkInfoElement();
    assertTrue(!!networkInfoElement);

    return dx_utils.getEthernetInfoElement(networkInfoElement);
  }

  /** @return {!Element} */
  function getWifiInfoElement() {
    const networkInfoElement = getNetworkInfoElement();
    assertTrue(!!networkInfoElement);

    return dx_utils.getWifiInfoElement(networkInfoElement);
  }

  /** @return {!Array<string>} */
  function getNameServers() {
    return /** @type {!Array<string>} */ (
        networkCardElement.network.ipConfig.nameServers);
  }

  /** @return {!Promise} */
  function openIpConfigDrawer() {
    networkCardElement.$$('#ipConfigInfoDrawer').$$('#drawerToggle').click();
    return flushTasks();
  }

  /**
   * Get timerId_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {number}
   */
  function getTimerId() {
    return networkCardElement.timerId_;
  }

  test('CardTitleWiFiConnectedInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getWifiInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('CardTitleWiFiDisabledInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuidDisabled').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('WifiDisconnectedShowTroubleShooting', () => {
    return initializeNetworkCard('wifiDisconnectedGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('WifiPortalShowTroubleShooting', () => {
    return initializeNetworkCard('wifiPortalGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getNetworkInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeNetworkCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Ethernet');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getEthernetInfoElement()));
    });
  });

  test('EthernetDisconnectedShowTroubleShooting', () => {
    return initializeNetworkCard('ethernetDisconnectedGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Ethernet');
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('NetworkConnectingHideTroubleShooting', () => {
    return initializeNetworkCard('ethernetConnectingGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.$$('#cardTitle'), 'Ethernet');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getNetworkInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('CardDrawerInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuid').then(() => {
      const ipConfigInfoDrawerElement =
          /** @type {!IpConfigInfoDrawerElement} */ (
              networkCardElement.$$('#ipConfigInfoDrawer'));
      assertTrue(
          isVisible(/** @type {!HTMLElement} */ (ipConfigInfoDrawerElement)));
      assertDeepEquals(fakeWifiNetwork, ipConfigInfoDrawerElement.network);
    });
  });

  test('InvalidNameServersFilteredCorrectly', () => {
    return initializeNetworkCard('wifiGuidInvalidNameServers')
        .then(() => openIpConfigDrawer())
        .then(() => {
          assertFalse(getNameServers().includes('0.0.0.0'));
          const ipConfigInfoDrawerElement =
              /** @type {!IpConfigInfoDrawerElement} */ (
                  networkCardElement.$$('#ipConfigInfoDrawer'));
          // Valid name server should not have been filtered from the list.
          assertEquals(
              dx_utils.getDataPointValue(
                  ipConfigInfoDrawerElement, '#nameServers'),
              '192.168.86.1');
        });
  });

  test('TimerResetsOnNetworkChange', () => {
    return initializeNetworkCard('wifiGuidNoIpAddress')
        .then(() => {
          // Timer should be in progress since this network is missing an
          // IP Address.
          assertTrue(getTimerId() !== -1);
        })
        .then(() => changeGuid('ethernetGuid'))
        .then(() => {
          // After a network change event, the timer should have been cleared
          // and reset.
          assertTrue(getTimerId() === -1);
        });
  });
}
