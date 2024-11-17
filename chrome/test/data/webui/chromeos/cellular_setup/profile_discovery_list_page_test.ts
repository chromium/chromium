// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import 'chrome://resources/ash/common/cellular_setup/profile_discovery_list_page.js';

import type {EsimFlowUiElement} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import type {ProfileDiscoveryListPageElement} from 'chrome://resources/ash/common/cellular_setup/profile_discovery_list_page.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import {FakeESimManagerRemote} from './fake_esim_manager_remote.js';

suite('CrComponentsProfileDiscoveryListPageTest', function() {
  let eSimManagerRemote: FakeESimManagerRemote;
  let networkConfigRemote: FakeNetworkConfig;
  let eSimPage: EsimFlowUiElement;
  let profileDiscoveryPage: ProfileDiscoveryListPageElement|null;

  async function init(isCarrierLocked: boolean) {
    networkConfigRemote.setDeviceStateForTest({
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: undefined,
      scanning: false,
      simLockStatus: undefined,
      simInfos: undefined,
      inhibitReason: InhibitReason.kNotInhibited,
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: isCarrierLocked,
      isFlashing: false,
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
    });
    await flushTasks();

    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(eSimPage);
    await flushTasks();
    assertTrue(!!eSimPage.shadowRoot);
    profileDiscoveryPage =
        eSimPage.shadowRoot.querySelector('#profileDiscoveryPage');
  }

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigRemote);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
  });

  [true, false].forEach(isCarrierLocked => {
    test('Show/hide Carrier lock warning', async function() {
      await init(isCarrierLocked);
      assertTrue(!!profileDiscoveryPage);
      if (isCarrierLocked) {
        assertTrue(!!profileDiscoveryPage.shadowRoot!.querySelector(
            '#carrierLockWarningContainer'));
      } else {
        assertFalse(!!profileDiscoveryPage.shadowRoot!.querySelector(
            '#carrierLockWarningContainer'));
      }
    });
  });
});
