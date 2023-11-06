// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cfm-network-settings/cfm_network_settings.js';

import {CfmNetworkSettingsBrowserProxyImpl} from 'chrome://cfm-network-settings/cfm_network_settings_browser_proxy.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {CfmNetworkSettingsBrowserProxy} */
export class TestCfmNetworkSettingsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showNetworkConfig',
      'showNetworkDetails',
      'showAddWifi',
      'showManageCerts',
    ]);
  }

  /** @override */
  showNetworkConfig(guid) {
    this.methodCalled('showNetworkConfig', guid);
  }

  /** @override */
  showNetworkDetails(guid) {
    this.methodCalled('showNetworkDetails', guid);
  }

  /** @override */
  showAddWifi() {
    this.methodCalled('showAddWifi');
  }

  /** @override */
  showManageCerts() {
    this.methodCalled('showManageCerts');
  }
}

suite('cfm-network-settings', () => {
  const wiFiId = 'WiFi 1';
  const disconnectedWiFiId = 'WiFi 2';
  const configuredWiFiId = 'Configured WiFi';
  let mojoApi;
  let networkSettings;
  let browserProxy;

  async function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  suiteSetup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi;
  });

  setup(async () => {
    PolymerTest.clearBody();

    browserProxy = new TestCfmNetworkSettingsBrowserProxy();
    CfmNetworkSettingsBrowserProxyImpl.setInstance(browserProxy);

    mojoApi.resetForTest();

    const connectedWiFi =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, wiFiId);
    connectedWiFi.connectable = true;
    connectedWiFi.connectionState = ConnectionStateType.kConnected;

    const disconnectedWiFi =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, disconnectedWiFiId);

    const configuredWiFi =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, configuredWiFiId);
    configuredWiFi.connectable = true;

    mojoApi.addNetworksForTest(
        [connectedWiFi, disconnectedWiFi, configuredWiFi]);
    mojoApi.removeNetworkForTest({guid: 'eth0_guid'});
    mojoApi.setDeviceStateForTest({
      type: NetworkType.kWiFi,
      deviceState: DeviceStateType.kEnabled,
    });

    networkSettings = document.createElement('cfm-network-settings');
    networkSettings.style.height = '100%';
    networkSettings.style.width = '100%';
    document.body.appendChild(networkSettings);
    const networkSelect = networkSettings.$['network-select'];
    networkSelect.refreshNetworks();

    await flushAsync();
  });

  test('Show add wifi', () => {
    const customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    const items =
        customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    const clickable = items[0].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertEquals(1, browserProxy.getCallCount('showAddWifi'));
  });

  test('Show proxy settings', () => {
    const customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    const items =
        customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    const clickable = items[1].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertDeepEquals([''], browserProxy.getArgs('showNetworkDetails'));
  });

  test('Show manage certs', () => {
    const customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    const items =
        customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    const clickable = items[2].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertEquals(1, browserProxy.getCallCount('showManageCerts'));
  });

  test('Click unconnected network', () => {
    const networkSelect = networkSettings.$['network-select'];
    const item =
        networkSelect.getNetworkListItemByNameForTest(disconnectedWiFiId);
    item.click();
    assertDeepEquals(
        [disconnectedWiFiId + '_guid'],
        browserProxy.getArgs('showNetworkConfig'));
  });

  test('Click connected network', () => {
    const networkSelect = networkSettings.$['network-select'];
    const item = networkSelect.getNetworkListItemByNameForTest(wiFiId);
    item.click();
    assertDeepEquals(
        [wiFiId + '_guid'], browserProxy.getArgs('showNetworkDetails'));
  });

  test('Click configured network', async () => {
    // FakeNetworkConfig doesn't let us mock out startConnect to do
    // anything other than return canceled, so we can't verify the
    // kNotConfigured case directly, only check that startConnect was called.
    let connectTried = false;
    mojoApi.whenCalled('startConnect').then(() => connectTried = true);

    const networkSelect = networkSettings.$['network-select'];
    const item =
        networkSelect.getNetworkListItemByNameForTest(configuredWiFiId);
    item.click();

    await flushAsync();

    assertTrue(connectTried);
  });
});
