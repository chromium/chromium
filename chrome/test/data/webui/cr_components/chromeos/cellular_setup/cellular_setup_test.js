// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';

// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue, assertFalse} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// clang-format on

suite('CrComponentsCellularSetupTest', function() {
  let cellularSetupPage;
  let eSimManagerRemote;
  let networkConfigRemote;

  setup(function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    networkConfigRemote = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigRemote;
  });

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function init() {
    const mojom = chromeos.networkConfig.mojom;
    networkConfigRemote.setDeviceStateForTest({
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      simInfos: [{slot_id: 0, iccid: '1111111111111111'}],
    });
    eSimManagerRemote.addEuiccForTest(2);
    Polymer.dom.flush();

    cellularSetupPage = document.createElement('cellular-setup');
    cellularSetupPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(cellularSetupPage);
    Polymer.dom.flush();
  }

  test('Show pSim flow ui', async function() {
    init();
    await flushAsync();
    let eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    let pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    assertTrue(!!eSimFlow);
    assertFalse(!!pSimFlow);

    cellularSetupPage.currentPageName =
        cellularSetup.CellularSetupPageName.PSIM_FLOW_UI;
    await flushAsync();
    eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    assertFalse(!!eSimFlow);
    assertTrue(!!pSimFlow);
  });

  test('Show eSIM flow ui', async function() {
    init();
    await flushAsync();
    let eSimFlow = cellularSetupPage.$$('esim-flow-ui');
    let pSimFlow = cellularSetupPage.$$('psim-flow-ui');

    // By default eSIM flow is always shown
    assertTrue(!!eSimFlow);
    assertFalse(!!pSimFlow);
  });
});
