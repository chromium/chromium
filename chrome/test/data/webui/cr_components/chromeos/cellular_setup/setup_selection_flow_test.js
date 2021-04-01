// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/setup_selection_flow.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// clang-format on

suite('CrComponentsSetupSelectionFlowTest', function() {
  let setupSelectionFlow;
  let networkConfigRemote;

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    networkConfigRemote = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ =
        networkConfigRemote;

    setupSelectionFlow = document.createElement('setup-selection-flow');
    setupSelectionFlow.delegate =
        new cellular_setup.FakeCellularSetupDelegate();
    setupSelectionFlow.initSubflow();
    document.body.appendChild(setupSelectionFlow);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const crRadio = setupSelectionFlow.$$('cr-radio-group');
    assertTrue(!!crRadio);
  });

  test(
      'Disable eSIM flow button if not connected to non-cellular network',
      async function() {
        assertTrue(setupSelectionFlow.$.esimFlowUiBtn.disabled);

        const wifiNetwork = OncMojo.getDefaultNetworkState(
            chromeos.networkConfig.mojom.NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState =
            chromeos.networkConfig.mojom.ConnectionStateType.kOnline;
        networkConfigRemote.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        assertFalse(setupSelectionFlow.$.esimFlowUiBtn.disabled);
        assertFalse(!!setupSelectionFlow.$$('iron-icon'));

        networkConfigRemote.removeNetworkForTest(wifiNetwork);
        await flushAsync();

        assertTrue(setupSelectionFlow.$.esimFlowUiBtn.disabled);
        assertTrue(!!setupSelectionFlow.$$('iron-icon'));
      });
});
