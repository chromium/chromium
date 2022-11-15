// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/cellular_setup.js';
import 'chrome://resources/ash/common/cellular_setup/psim_flow_ui.js';

import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';

import {assertFalse, assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';

suite('CrComponentsCellularSetupTest', function() {
  let cellularSetupPage;
  let eSimManagerRemote;
  let networkConfigRemote;

  setup(function() {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    networkConfigRemote = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigRemote;
  });

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function init() {
    networkConfigRemote.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
    });
    eSimManagerRemote.addEuiccForTest(2);
    flush();

    cellularSetupPage = document.createElement('cellular-setup');
    cellularSetupPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(cellularSetupPage);
    flush();
  }

  test('Show pSim flow ui', async function() {
    init();
    await flushAsync();
    let eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    let pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    assertTrue(!!eSimFlow);
    assertFalse(!!pSimFlow);

    cellularSetupPage.currentPageName = CellularSetupPageName.PSIM_FLOW_UI;
    await flushAsync();
    eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    assertFalse(!!eSimFlow);
    assertTrue(!!pSimFlow);
  });

  test('Show eSIM flow ui', async function() {
    init();
    await flushAsync();
    const eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    const pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    // By default eSIM flow is always shown
    assertTrue(!!eSimFlow);
    assertFalse(!!pSimFlow);
  });
});
