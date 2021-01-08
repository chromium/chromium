// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// clang-format on

suite('CellularNetworkList', function() {
  let cellularNetworkList;

  let mojom;

  /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  let eSimManagerRemote;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);
  });

  function init() {
    cellularNetworkList = document.createElement('cellular-networks-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    cellularNetworkList.style.height = '100%';
    cellularNetworkList.style.width = '100%';
    document.body.appendChild(cellularNetworkList);
    Polymer.dom.flush();
  }

  function setNetworksForTest(type, networks) {
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(type, true);
    mojoApi_.addNetworksForTest(networks);
    cellularNetworkList.deviceState = mojoApi_.getDeviceStateForTest(type);
    cellularNetworkList.networks = networks;
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Tether, cellular and eSIM profiles', async () => {
    init();

    const eSimNetwork1 = OncMojo.getDefaultNetworkState(
        mojom.NetworkType.kCellular, 'cellular_esim1');
    const eSimNetwork2 = OncMojo.getDefaultNetworkState(
        mojom.NetworkType.kCellular, 'cellular_esim2');
    setNetworksForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, 'cellular2'),
      eSimNetwork1,
      eSimNetwork2,
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
    ]);

    const eSimManagedProperties1 = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, eSimNetwork1.guid, eSimNetwork1.name);
    const eSimManagedProperties2 = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, eSimNetwork2.guid, eSimNetwork2.name);
    eSimManagedProperties1.typeProperties.cellular.eid =
        '11111111111111111111111111111111';
    eSimManagedProperties2.typeProperties.cellular.eid =
        '22222222222222222222222222222222';
    mojoApi_.setManagedPropertiesForTest(eSimManagedProperties1);
    mojoApi_.setManagedPropertiesForTest(eSimManagedProperties2);

    eSimManagerRemote.addEuiccForTest(2);

    await flushAsync();

    const eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertTrue(!!eSimNetworkList);

    const pSimNetworkList = cellularNetworkList.$$('#psimNetworkList');
    assertTrue(!!pSimNetworkList);

    const tetherNetworkList = cellularNetworkList.$$('#tetherNetworkList');
    assertTrue(!!tetherNetworkList);

    assertEquals(2, eSimNetworkList.networks.length);
    assertEquals(2, pSimNetworkList.networks.length);
    assertEquals(2, tetherNetworkList.networks.length);
    assertEquals(2, eSimNetworkList.customItems.length);
  });
  test(
      'Fire show cellular setup event on eSim/psim no network link click',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        init();

        setNetworksForTest(mojom.NetworkType.kCellular, [
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        ]);
        Polymer.dom.flush();

        await flushAsync();
        const esimNoNetworkAnchor =
            cellularNetworkList.$$('#eSimNoNetworkFound')
                .querySelector('settings-localized-link')
                .shadowRoot.querySelector('a');
        assertTrue(!!esimNoNetworkAnchor);

        const showEsimCellularSetupPromise = test_util.eventToPromise(
            'show-cellular-setup', cellularNetworkList);
        esimNoNetworkAnchor.click();
        const eSimCellularEvent = await showEsimCellularSetupPromise;
        assertEquals(
            eSimCellularEvent.detail.pageName,
            cellularSetup.CellularSetupPageName.ESIM_FLOW_UI);


        const psimNoNetworkAnchor =
            cellularNetworkList.$$('#pSimNoNetworkFound')
                .querySelector('settings-localized-link')
                .shadowRoot.querySelector('a');
        assertTrue(!!psimNoNetworkAnchor);
        const showPsimCellularSetupPromise = test_util.eventToPromise(
            'show-cellular-setup', cellularNetworkList);
        psimNoNetworkAnchor.click();
        const pSimCellularEvent = await showPsimCellularSetupPromise;
        assertEquals(
            pSimCellularEvent.detail.pageName,
            cellularSetup.CellularSetupPageName.PSIM_FLOW_UI);
      });

  test('Show EID and QR code popup', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    let eidPopup = cellularNetworkList.$$('.eid-popup');
    assertFalse(!!eidPopup);
    const eidPopupBtn = cellularNetworkList.$$('#eidPopupButton');
    assertTrue(!!eidPopupBtn);

    eidPopupBtn.click();
    await flushAsync();

    eidPopup = cellularNetworkList.$$('.eid-popup');
    assertTrue(!!eidPopup);
  });

  test('Install pending eSIM profile', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    await flushAsync();

    let eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertTrue(!!eSimNetworkList);

    Polymer.dom.flush();

    const listItem = eSimNetworkList.$$('network-list-item');
    assertTrue(!!listItem);
    const installButton = listItem.$$('#installButton');
    assertTrue(!!installButton);
    installButton.click();

    await flushAsync();

    // eSIM network list should now be hidden and link showing.
    eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertFalse(!!eSimNetworkList);
    const esimNoNetworkAnchor = cellularNetworkList.$$('#eSimNoNetworkFound')
                                    .querySelector('settings-localized-link')
                                    .shadowRoot.querySelector('a');
    assertTrue(!!esimNoNetworkAnchor);
  });
  test('Hide esim section when no EUICC is found', async () => {
    setNetworksForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
    ]);
    init();
    Polymer.dom.flush();
    await flushAsync();
    const esimNetworkList = cellularNetworkList.$$('#esimNetworkList');

    assertFalse(!!esimNetworkList);
  });

});
