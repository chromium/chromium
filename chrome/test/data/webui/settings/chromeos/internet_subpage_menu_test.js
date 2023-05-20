// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';

suite('InternetSubpageMenu', function() {
  let internetSubpageMenu;
  let mojoApi_;
  let eSimManagerRemote;

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  async function init(deviceState) {
    internetSubpageMenu =
        document.createElement('settings-internet-subpage-menu');
    internetSubpageMenu.deviceState = deviceState;
    document.body.appendChild(internetSubpageMenu);
    await flushAsync();
  }

  setup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
  });

  test('Do not show subpage menu for non-cellular networks', async function() {
    const wifiDeviceState = mojoApi_.getDeviceStateForTest(NetworkType.kWiFi);
    await init(wifiDeviceState);
    let tripleDot =
        internetSubpageMenu.shadowRoot.querySelector('#moreNetworkMenuButton');
    assertEquals(null, tripleDot);

    const cellularDeviceState =
        mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    cellularDeviceState.imei = '1234567890';
    internetSubpageMenu.deviceState = cellularDeviceState;
    await flushAsync();
    tripleDot =
        internetSubpageMenu.shadowRoot.querySelector('#moreNetworkMenuButton');
    assertNotEquals(null, tripleDot);
  });

  test('Show Device Info dialog when menu item is clicked', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    const cellularDeviceState =
        mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
    cellularDeviceState.imei = '1234567890';
    await init(cellularDeviceState);
    const tripleDot =
        internetSubpageMenu.shadowRoot.querySelector('#moreNetworkMenuButton');
    assertNotEquals(null, tripleDot);

    tripleDot.click();
    await flushAsync();

    const actionMenu =
        internetSubpageMenu.shadowRoot.querySelector('cr-action-menu');
    assertNotEquals(null, actionMenu);
    assertTrue(actionMenu.open);

    const deviceInfoMenuItem = actionMenu.querySelector('#deviceInfoMenuItem');
    assertNotEquals(null, deviceInfoMenuItem);

    deviceInfoMenuItem.click();
    await flushAsync();

    const deviceInfoDialog = internetSubpageMenu.shadowRoot.querySelector(
        'network-device-info-dialog');
    assertNotEquals(null, deviceInfoDialog);
  });
});