// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cfm-network-settings/cfm_network_settings.js';

import {CfmNetworkSettingsBrowserProxyImpl} from 'chrome://cfm-network-settings/cfm_network_settings_browser_proxy.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

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
  const mojom = chromeos.networkConfig.mojom;
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
    CfmNetworkSettingsBrowserProxyImpl.instance_ = browserProxy;

    mojoApi.resetForTest();

    let connectedWiFi =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, wiFiId);
    connectedWiFi.connectable = true;
    connectedWiFi.connectionState = mojom.ConnectionStateType.kConnected;

    let disconnectedWiFi = OncMojo.getDefaultNetworkState(
        mojom.NetworkType.kWiFi, disconnectedWiFiId);

    let configuredWiFi = OncMojo.getDefaultNetworkState(
        mojom.NetworkType.kWiFi, configuredWiFiId);
    configuredWiFi.connectable = true;

    mojoApi.addNetworksForTest(
        [connectedWiFi, disconnectedWiFi, configuredWiFi]);
    mojoApi.removeNetworkForTest({guid: 'eth0_guid'});
    mojoApi.setDeviceStateForTest({
      type: mojom.NetworkType.kWiFi,
      deviceState: mojom.DeviceStateType.kEnabled,
    });

    networkSettings = document.createElement('cfm-network-settings');
    networkSettings.style.height = '100%';
    networkSettings.style.width = '100%';
    document.body.appendChild(networkSettings);
    let networkSelect = networkSettings.$['network-select'];
    networkSelect.refreshNetworks();

    await flushAsync();
  });

  test('Show add wifi', () => {
    let customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    let items = customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    let clickable = items[0].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertEquals(1, browserProxy.getCallCount('showAddWifi'));
  });

  test('Show proxy settings', () => {
    let customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    let items = customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    let clickable = items[1].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertDeepEquals([''], browserProxy.getArgs('showNetworkDetails'));
  });

  test('Show manage certs', () => {
    let customItemList = networkSettings.$['custom-items'];
    // While the network list items do have the correct ARIA role, clicking them
    // directly does nothing; we have to click a specific element within them.
    let items = customItemList.shadowRoot.querySelectorAll('*[role="button"]');
    let clickable = items[2].shadowRoot.querySelectorAll('*[role="button"]');
    clickable[0].click();
    assertEquals(1, browserProxy.getCallCount('showManageCerts'));
  });

  test('Click unconnected network', () => {
    let networkSelect = networkSettings.$['network-select'];
    let item =
        networkSelect.getNetworkListItemByNameForTest(disconnectedWiFiId);
    item.click();
    assertDeepEquals(
        [disconnectedWiFiId + '_guid'],
        browserProxy.getArgs('showNetworkConfig'));
  });

  test('Click connected network', () => {
    let networkSelect = networkSettings.$['network-select'];
    let item = networkSelect.getNetworkListItemByNameForTest(wiFiId);
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

    let networkSelect = networkSettings.$['network-select'];
    let item = networkSelect.getNetworkListItemByNameForTest(configuredWiFiId);
    item.click();

    await flushAsync();

    assertTrue(connectTried);
  });
});
