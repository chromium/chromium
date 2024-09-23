// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Strings needs to be imported before network_card to ensure assert is not
// triggered during test.
import 'chrome://diagnostics/strings.m.js';
import 'chrome://diagnostics/network_card.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {CellularInfoElement} from 'chrome://diagnostics/cellular_info.js';
import {DiagnosticsNetworkIconElement} from 'chrome://diagnostics/diagnostics_network_icon.js';
import {EthernetInfoElement} from 'chrome://diagnostics/ethernet_info.js';
import {fakeCellularDisabledNetwork, fakeCellularDisconnectedNetwork, fakeCellularNetwork, fakeCellularWithIpConfigNetwork, fakeConnectingEthernetNetwork, fakeDisconnectedEthernetNetwork, fakeDisconnectedWifiNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakePortalWifiNetwork, fakeWifiNetwork, fakeWifiNetworkDisabled, fakeWifiNetworkInvalidNameServers, fakeWifiNetworkNoIpAddress} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {IpConfigInfoDrawerElement} from 'chrome://diagnostics/ip_config_info_drawer.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {NetworkCardElement} from 'chrome://diagnostics/network_card.js';
import {NetworkInfoElement} from 'chrome://diagnostics/network_info.js';
import {NetworkTroubleshootingElement} from 'chrome://diagnostics/network_troubleshooting.js';
import {WifiInfoElement} from 'chrome://diagnostics/wifi_info.js';
import {CrExpandButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkCardTestSuite', function() {
  let networkCardElement: NetworkCardElement|null = null;

  const provider: FakeNetworkHealthProvider = new FakeNetworkHealthProvider();

  suiteSetup(() => {
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    networkCardElement?.remove();
    networkCardElement = null;
    provider.reset();
  });

  function initializeNetworkCard(
      guid: string, timeout: number|undefined = undefined): Promise<void> {
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
    networkCardElement = document.createElement('network-card');
    assert(networkCardElement);
    networkCardElement.guid = guid;
    if (timeout || timeout === 0) {
      networkCardElement.setTimeoutInMsForTesting(timeout);
    }
    document.body.appendChild(networkCardElement);

    return flushTasks();
  }

  function changeGuid(guid: string): Promise<void> {
    assert(networkCardElement);
    networkCardElement.guid = guid;
    return flushTasks();
  }

  function getTroubleConnectingElement(): NetworkTroubleshootingElement|null {
    assert(networkCardElement);
    return networkCardElement.shadowRoot!
        .querySelector<NetworkTroubleshootingElement>(
            '#networkTroubleshooting');
  }

  function getIpConfigDrawerElement(): IpConfigInfoDrawerElement|null {
    assert(networkCardElement);
    return networkCardElement.shadowRoot!
        .querySelector<IpConfigInfoDrawerElement>('#ipConfigInfoDrawer');
  }

  function getNetworkInfoElement(): NetworkInfoElement|null {
    assert(networkCardElement);
    return networkCardElement.shadowRoot!.querySelector('network-info');
  }

  function getNetworkIcon(): DiagnosticsNetworkIconElement|null {
    assert(networkCardElement);
    return networkCardElement.shadowRoot!
        .querySelector<DiagnosticsNetworkIconElement>('#icon');
  }

  function getCellularInfoElement(): CellularInfoElement {
    const networkInfoElement = getNetworkInfoElement();
    assert(networkInfoElement);

    const cellularInfo = dx_utils.getCellularInfoElement(networkInfoElement);
    assert(cellularInfo);
    return cellularInfo;
  }

  function getEthernetInfoElement(): EthernetInfoElement {
    const networkInfoElement = getNetworkInfoElement();
    assert(networkInfoElement);

    const ethernetInfo = dx_utils.getEthernetInfoElement(networkInfoElement);
    assert(ethernetInfo);
    return ethernetInfo;
  }

  function getWifiInfoElement(): WifiInfoElement {
    const networkInfoElement = getNetworkInfoElement();
    assert(networkInfoElement);

    const wifiInfo = dx_utils.getWifiInfoElement(networkInfoElement);
    assert(wifiInfo);
    return wifiInfo;
  }

  function getNameServers(): string[] {
    assert(networkCardElement);
    return networkCardElement.network!.ipConfig!.nameServers as string[];
  }

  function openIpConfigDrawer(): Promise<void> {
    const drawer = getIpConfigDrawerElement();
    const toggle = drawer!.shadowRoot!.querySelector<CrExpandButtonElement>(
        '#drawerToggle');
    assert(toggle);
    toggle.click();
    return flushTasks();
  }

  function getTimerId(): number {
    assert(networkCardElement);
    return networkCardElement.getTimerIdForTesting();
  }

  function getTroubleshootingHeader(): string {
    const troubleConnecting = getTroubleConnectingElement();
    assert(troubleConnecting);
    return troubleConnecting.troubleshootingInfo.header;
  }

  function getTroubleshootingLinkText(): string {
    const troubleConnecting = getTroubleConnectingElement();
    assert(troubleConnecting);
    return troubleConnecting.troubleshootingInfo.linkText;
  }

  function getUnableToObtainIpAddress(): boolean {
    assert(networkCardElement);
    return networkCardElement.getUnableToObtainIpAddressForTesting();
  }

  test('CardTitleWiFiConnectedInitializedCorrectly', async () => {
    await initializeNetworkCard('wifiGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Wi-Fi');
    assertTrue(isVisible(getNetworkIcon()));
    assertFalse(isVisible(getTroubleConnectingElement()));
    assertTrue(isVisible(getWifiInfoElement()));
    assertTrue(isVisible(getIpConfigDrawerElement()));
  });

  test('CardTitleWiFiDisabledInitializedCorrectly', async () => {
    await initializeNetworkCard('wifiGuidDisabled');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Wi-Fi');
    assertTrue(isVisible(getTroubleConnectingElement()));
    assertFalse(isVisible(getNetworkInfoElement()));
    assertFalse(isVisible(getIpConfigDrawerElement()));
    assertEquals(
        networkCardElement.i18n('joinNetworkLinkText', 'Wi-Fi'),
        getTroubleshootingLinkText());
  });

  test('WifiDisconnectedShowTroubleShooting', async () => {
    const networkType = 'Wi-Fi';
    await initializeNetworkCard('wifiDisconnectedGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
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

  test('WifiPortalShowTroubleShooting', async () => {
    await initializeNetworkCard('wifiPortalGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Wi-Fi');
    assertTrue(isVisible(getNetworkIcon()));
    assertTrue(isVisible(getTroubleConnectingElement()));
    assertTrue(isVisible(getNetworkInfoElement()));
    assertTrue(isVisible(getIpConfigDrawerElement()));
    const troubleConnecting = getTroubleConnectingElement();
    assert(troubleConnecting);
    dx_utils.assertTextContains(
        loadTimeData.getStringF('troubleshootingText', 'Wi-Fi'),
        troubleConnecting.troubleshootingInfo.header);
  });

  test('CardTitleEthernetOnlineInitializedCorrectly', async () => {
    await initializeNetworkCard('ethernetGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Ethernet');
    assertTrue(isVisible(getNetworkIcon()));
    assertFalse(isVisible(getTroubleConnectingElement()));
    assertTrue(isVisible(getEthernetInfoElement()));
  });

  test('EthernetDisconnectedShowTroubleShooting', async () => {
    const networkType = 'Ethernet';
    await initializeNetworkCard('ethernetDisconnectedGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
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

  test('NetworkConnectingHideTroubleShooting', async () => {
    await initializeNetworkCard('ethernetConnectingGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Ethernet');
    assertTrue(isVisible(getNetworkIcon()));
    assertFalse(isVisible(getTroubleConnectingElement()));
    assertTrue(isVisible(getNetworkInfoElement()));
    assertTrue(isVisible(getIpConfigDrawerElement()));
  });

  test('CardDrawerInitializedCorrectly', async () => {
    await initializeNetworkCard('wifiGuid');
    const ipConfigInfoDrawerElement = getIpConfigDrawerElement();
    assert(ipConfigInfoDrawerElement);
    assertTrue(isVisible(ipConfigInfoDrawerElement));
    assertDeepEquals(fakeWifiNetwork, ipConfigInfoDrawerElement.network);
  });

  test('InvalidNameServersFilteredCorrectly', async () => {
    await initializeNetworkCard('wifiGuidInvalidNameServers');
    await openIpConfigDrawer();
    assertFalse(getNameServers().includes('0.0.0.0'));
    const ipConfigInfoDrawerElement = getIpConfigDrawerElement();
    // Valid name server should not have been filtered from the list.
    assertEquals(
        dx_utils.getDataPointValue(ipConfigInfoDrawerElement, '#nameServers'),
        '192.168.86.1');
  });

  test('TimerResetsOnNetworkChange', async () => {
    await initializeNetworkCard('wifiGuidNoIpAddress', 0);
    assert(networkCardElement);
    assertEquals('wifiGuidNoIpAddress', networkCardElement.guid);
    // Timer should be in progress since this network is missing an
    // IP Address.
    assertTrue(getTimerId() !== -1);
    await changeGuid('ethernetGuid');
    assert(networkCardElement);
    assertEquals('ethernetGuid', networkCardElement.guid);
    // After a network change event, the timer should have been cleared
    // and reset.
    assertEquals(-1, getTimerId());
  });

  test('IpMissingShowsTroubleshootingAfterDelay', async () => {
    await initializeNetworkCard('wifiGuidNoIpAddress', 0);
    await flushTasks();
    assertTrue(isVisible(getTroubleConnectingElement()));
    // Verify banner header and link text.
    dx_utils.assertTextContains(
        loadTimeData.getString('noIpAddressText'), getTroubleshootingHeader());
    dx_utils.assertTextContains(
        loadTimeData.getString('visitSettingsToConfigureLinkText'),
        getTroubleshootingLinkText());
    // Timer should have been cleared.
    assertTrue(getTimerId() === -1);
    await changeGuid('ethernetGuid');
    // Flag should have been reset.
    assertFalse(getUnableToObtainIpAddress());
    // After a network change event, the timer should have been cleared
    // and reset.
    assertTrue(getTimerId() === -1);
  });

  test('CardTitleCellularConnectedInitializedCorrectly', async () => {
    await initializeNetworkCard('cellularWithIpConfigGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Mobile data');
    assertTrue(isVisible(getNetworkIcon()));
    assertFalse(isVisible(getTroubleConnectingElement()));
    assertTrue(isVisible(getCellularInfoElement()));
    assertTrue(isVisible(getIpConfigDrawerElement()));
  });

  test('CardTitleCellularDisabledInitializedCorrectly', async () => {
    await initializeNetworkCard('cellularDisabledGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
        'Mobile data');
    assertTrue(isVisible(getTroubleConnectingElement()));
    assertFalse(isVisible(getNetworkInfoElement()));
    assertFalse(isVisible(getIpConfigDrawerElement()));
    assertEquals(
        networkCardElement.i18n('reconnectLinkText'),
        getTroubleshootingLinkText());
  });

  test('CardTitleCellularDisconnectedInitializedCorrectly', async () => {
    const networkType = 'Mobile data';
    await initializeNetworkCard('cellularDisconnectedGuid');
    assert(networkCardElement);
    dx_utils.assertElementContainsText(
        strictQuery(
            '#cardTitle', networkCardElement.shadowRoot, HTMLDivElement),
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
