// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsInternetSubpageMenuElement} from 'chrome://os-settings/lazy_load.js';
import {CrActionMenuElement} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-internet-subpage-menu>', () => {
  let internetSubpageMenu: SettingsInternetSubpageMenuElement;
  let mojoApi: FakeNetworkConfig;
  let eSimManagerRemote: FakeESimManagerRemote;

  async function init(deviceState: OncMojo.DeviceStateProperties):
      Promise<void> {
    internetSubpageMenu =
        document.createElement('settings-internet-subpage-menu');
    internetSubpageMenu.deviceState = deviceState;
    document.body.appendChild(internetSubpageMenu);
    await flushTasks();
  }

  setup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    mojoApi.resetForTest();

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);

    mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
  });

  teardown(() => {
    internetSubpageMenu.remove();
  });

  test('Do not show subpage menu for non-cellular networks', async () => {
    const wifiDeviceState = mojoApi.getDeviceStateForTest(NetworkType.kWiFi);
    assertTrue(!!wifiDeviceState);
    await init(wifiDeviceState);
    let tripleDot =
        internetSubpageMenu.shadowRoot!.querySelector('#moreNetworkMenuButton');
    assertNull(tripleDot);

    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.imei = '1234567890';
    internetSubpageMenu.deviceState = cellularDeviceState;
    await flushTasks();
    tripleDot =
        internetSubpageMenu.shadowRoot!.querySelector('#moreNetworkMenuButton');
    assertTrue(isVisible(tripleDot));
  });

  test('Show Device Info dialog when menu item is clicked', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    const cellularDeviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular);
    assertTrue(!!cellularDeviceState);
    cellularDeviceState.imei = '1234567890';
    await init(cellularDeviceState);
    const tripleDot =
        internetSubpageMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#moreNetworkMenuButton');
    assertTrue(!!tripleDot);

    tripleDot.click();
    await flushTasks();

    const actionMenu =
        internetSubpageMenu.shadowRoot!.querySelector<CrActionMenuElement>(
            'cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const deviceInfoMenuItem =
        actionMenu.querySelector<HTMLButtonElement>('#deviceInfoMenuItem');
    assertTrue(!!deviceInfoMenuItem);

    deviceInfoMenuItem.click();
    await flushTasks();

    const deviceInfoDialog = internetSubpageMenu.shadowRoot!.querySelector(
        'network-device-info-dialog');
    assertTrue(!!deviceInfoDialog);
  });
});
