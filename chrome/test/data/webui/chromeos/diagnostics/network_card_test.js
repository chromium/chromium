// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Strings needs to be imported before network_card to ensure assert is not
// triggered during test.
import 'chrome://diagnostics/strings.m.js';
import 'chrome://diagnostics/network_card.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeCellularDisabledNetwork, fakeCellularDisconnectedNetwork, fakeCellularNetwork, fakeCellularWithIpConfigNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeDisconnectedWifiNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled, fakeWifiNetworkInvalidNameServers, fakeWifiNetworkNoIpAddress} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {NetworkCardElement} from 'chrome://diagnostics/network_card.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkCardTestSuite', function() {
  /** @type {?NetworkCardElement} */
  let networkCardElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    networkCardElement.remove();
    networkCardElement = null;
    provider.reset();
  });

  /**
   * @param {string} guid
   * @param {?number} timeout
   */
  function initializeNetworkCard(guid, timeout = undefined) {
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
    provider.setFakeNetworkState(
        'cellularWithIpConfigGuid', [fakeCellularWithIpConfigNetwork]);
    provider.setFakeNetworkState(
        'cellularDisabledGuid', [fakeCellularDisabledNetwork]);
    provider.setFakeNetworkState(
        'cellularDisconnectedGuid', [fakeCellularDisconnectedNetwork]);
    // Add the network info to the DOM.
    networkCardElement = /** @type {!NetworkCardElement} */ (
        document.createElement('network-card'));
    assertTrue(!!networkCardElement);
    networkCardElement.guid = guid;
    if (timeout || timeout === 0) {
      /** @suppress {visibility} */
      networkCardElement.timeoutInMs = timeout;
    }
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
        networkCardElement.shadowRoot.querySelector('#networkTroubleshooting'));
  }

  /** @return {!Element} */
  function getIpConfigDrawerElement() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (
        networkCardElement.shadowRoot.querySelector('#ipConfigInfoDrawer'));
  }

  /** @return {!Element} */
  function getNetworkInfoElement() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (
        networkCardElement.shadowRoot.querySelector('network-info'));
  }

  /** @return {!Element} */
  function getNetworkIcon() {
    assertTrue(!!networkCardElement);

    return /** @type {!Element} */ (
        networkCardElement.shadowRoot.querySelector('#icon'));
  }

  /** @return {!Element} */
  function getCellularInfoElement() {
    const networkInfoElement = getNetworkInfoElement();
    assertTrue(!!networkInfoElement);

    return dx_utils.getCellularInfoElement(networkInfoElement);
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
    networkCardElement.shadowRoot.querySelector('#ipConfigInfoDrawer')
        .shadowRoot.querySelector('#drawerToggle')
        .click();
    return flushTasks();
  }

  /**
   * Get timerId_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {number}
   */
  function getTimerId() {
    return networkCardElement.timerId;
  }

  /** @return {string} */
  function getTroubleshootingHeader() {
    return getTroubleConnectingElement().troubleshootingInfo.header;
  }

  /** @return {string} */
  function getTroubleshootingLinkText() {
    return getTroubleConnectingElement().troubleshootingInfo.linkText;
  }

  /**
   * Get unableToObtainIpAddress_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {boolean}
   */
  function getUnableToObtainIpAddress() {
    return networkCardElement.unableToObtainIpAddress;
  }

  test('CardTitleWiFiConnectedInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getWifiInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('CardTitleWiFiDisabledInitializedCorrectly', () => {
    return initializeNetworkCard('wifiGuidDisabled').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
      assertEquals(
          networkCardElement.i18n('joinNetworkLinkText', 'Wi-Fi'),
          getTroubleshootingLinkText());
    });
  });

  test('WifiDisconnectedShowTroubleShooting', () => {
    const networkType = 'Wi-Fi';
    return initializeNetworkCard('wifiDisconnectedGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          networkType);
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
      assertEquals(
          networkCardElement.i18n('troubleshootingText', networkType),
          getTroubleshootingHeader());
      assertEquals(
          networkCardElement.i18n('troubleConnecting'),
          getTroubleshootingLinkText());
    });
  });

  test('WifiPortalShowTroubleShooting', () => {
    return initializeNetworkCard('wifiPortalGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'), 'Wi-Fi');
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getNetworkInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
      dx_utils.assertTextContains(
          loadTimeData.getStringF('troubleshootingText', 'Wi-Fi'),
          getTroubleConnectingElement().troubleshootingInfo.header);
    });
  });

  test('CardTitleEthernetOnlineInitializedCorrectly', () => {
    return initializeNetworkCard('ethernetGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          'Ethernet');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getEthernetInfoElement()));
    });
  });

  test('EthernetDisconnectedShowTroubleShooting', () => {
    const networkType = 'Ethernet';
    return initializeNetworkCard('ethernetDisconnectedGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          networkType);
      assertTrue(isVisible(getNetworkIcon()));
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
      assertEquals(
          networkCardElement.i18n('troubleshootingText', networkType),
          getTroubleshootingHeader());
      assertEquals(
          networkCardElement.i18n('troubleConnecting'),
          getTroubleshootingLinkText());
    });
  });

  test('NetworkConnectingHideTroubleShooting', () => {
    return initializeNetworkCard('ethernetConnectingGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          'Ethernet');
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
              networkCardElement.shadowRoot.querySelector(
                  '#ipConfigInfoDrawer'));
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
                  networkCardElement.shadowRoot.querySelector(
                      '#ipConfigInfoDrawer'));
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
          assertEquals('wifiGuidNoIpAddress', networkCardElement.guid);
          // Timer should be in progress since this network is missing an
          // IP Address.
          assertTrue(getTimerId() !== -1);
        })
        .then(() => changeGuid('ethernetGuid'))
        .then(() => {
          assertEquals('ethernetGuid', networkCardElement.guid);
          // After a network change event, the timer should have been cleared
          // and reset.
          assertEquals(-1, getTimerId());
        });
  });

  test('IpMissingShowsTroubleshootingAfterDelay', () => {
    return initializeNetworkCard('wifiGuidNoIpAddress', 0)
        .then(() => flushTasks())
        .then(() => {
          assertTrue(isVisible(getTroubleConnectingElement()));
          // Verify banner header and link text.
          dx_utils.assertTextContains(
              loadTimeData.getString('noIpAddressText'),
              getTroubleshootingHeader());
          dx_utils.assertTextContains(
              loadTimeData.getString('visitSettingsToConfigureLinkText'),
              getTroubleshootingLinkText());
          // Timer should have been cleared.
          assertTrue(getTimerId() === -1);
        })
        .then(() => changeGuid('ethernetGuid'))
        .then(() => {
          // Flag should have been reset.
          assertFalse(getUnableToObtainIpAddress());
          // After a network change event, the timer should have been cleared
          // and reset.
          assertTrue(getTimerId() === -1);
        });
  });

  test('CardTitleCellularConnectedInitializedCorrectly', () => {
    return initializeNetworkCard('cellularWithIpConfigGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          'Mobile data');
      assertTrue(isVisible(getNetworkIcon()));
      assertFalse(isVisible(getTroubleConnectingElement()));
      assertTrue(isVisible(getCellularInfoElement()));
      assertTrue(isVisible(getIpConfigDrawerElement()));
    });
  });

  test('CardTitleCellularDisabledInitializedCorrectly', () => {
    return initializeNetworkCard('cellularDisabledGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          'Mobile data');
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
      assertEquals(
          networkCardElement.i18n('reconnectLinkText'),
          getTroubleshootingLinkText());
    });
  });

  test('CardTitleCellularDisconnectedInitializedCorrectly', () => {
    const networkType = 'Mobile data';
    return initializeNetworkCard('cellularDisconnectedGuid').then(() => {
      dx_utils.assertElementContainsText(
          networkCardElement.shadowRoot.querySelector('#cardTitle'),
          networkType);
      assertTrue(isVisible(getTroubleConnectingElement()));
      assertFalse(isVisible(getNetworkInfoElement()));
      assertFalse(isVisible(getIpConfigDrawerElement()));
      assertEquals(
          networkCardElement.i18n('troubleshootingText', networkType),
          getTroubleshootingHeader());
      assertEquals(
          networkCardElement.i18n('troubleConnecting'),
          getTroubleshootingLinkText());
    });
  });
});
