// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_list_item.m.js';

// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {NetworkList} from 'chrome://resources/cr_components/chromeos/network/network_list_types.m.js';
// clang-format on

suite('NetworkListItemTest', function() {
  /** @type {!NetworkListItem|undefined} */
  let listItem;

  let mojom;

  let mojoApi_ = null;

  setup(function() {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });

    mojom = chromeos.networkConfig.mojom;
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    listItem = document.createElement('network-list-item');
    document.body.appendChild(listItem);
    Polymer.dom.flush();
  });

  function initCellularNetwork(guid, name, iccid, eid, homeProviderName) {
    const managedProperties = OncMojo.getDefaultManagedProperties(
        chromeos.networkConfig.mojom.NetworkType.kCellular, guid, name);
    managedProperties.typeProperties.cellular.homeProvider = {
      name: homeProviderName
    };
    managedProperties.typeProperties.cellular.eid = eid;
    mojoApi_.setManagedPropertiesForTest(managedProperties);

    const networkState =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, name);
    networkState.typeState.cellular.iccid = iccid;
    networkState.typeState.cellular.eid = eid;
    mojoApi_.addNetworksForTest([networkState]);
    return networkState;
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Network icon visibility', function() {
    // The network icon is not shown if there is no network state.
    let networkIcon = listItem.$$('network-icon');
    assertFalse(!!networkIcon);

    listItem.item = OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');

    // Update the network state.
    Polymer.dom.flush();

    // The network icon exists now.
    networkIcon = listItem.$$('network-icon');
    assertTrue(!!networkIcon);
  });

  test('Network provider name visibilty', async () => {
    listItem.item =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');
    await flushAsync();

    let providerName = listItem.$$('#subtitle');
    assertFalse(!!providerName.textContent.trim());

    const networkState = initCellularNetwork(
        'cellular1_guid', 'cellular1', '11111111111111111111', '123456',
        'Verizon Wireless');
    listItem.item = networkState;
    await flushAsync();

    providerName = listItem.$$('#subtitle');
    assertTrue(!!providerName);
    assertEquals('Verizon Wireless', providerName.textContent.trim());
  });

  test(
      'Pending eSIM profile name, provider, install button visibilty',
      async () => {
        const itemName = 'Item Name';
        const itemSubtitle = 'Item Subtitle';
        listItem.item = {
          customItemType: NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
          customItemName: itemName,
          customItemSubtitle: itemSubtitle,
          polymerIcon: 'network:cellular-0',
          showBeforeNetworksList: false,
          customData: {
            iccid: 'iccid',
          },
        };
        await flushAsync();

        let networkName = listItem.$$('#networkName');
        assertTrue(!!networkName);
        assertEquals(itemName, networkName.textContent.trim());

        let subtitle = listItem.$$('#subtitle');
        assertTrue(!!subtitle);
        assertEquals(itemSubtitle, subtitle.textContent.trim());

        let installButton = listItem.$$('#installButton');
        assertTrue(!!installButton);

        let installProfileEventIccid = null;
        listItem.addEventListener('install-profile', (event) => {
          installProfileEventIccid = event.detail.iccid;
        });
        installButton.click();

        await flushAsync();
        assertEquals(installProfileEventIccid, 'iccid');
      });

  test(
      'Installing eSIM profile name, provider, spinner visibilty', async () => {
        const itemName = 'Item Name';
        const itemSubtitle = 'Item Subtitle';
        listItem.item = {
          customItemType: NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE,
          customItemName: itemName,
          customItemSubtitle: itemSubtitle,
          polymerIcon: 'network:cellular-0',
          showBeforeNetworksList: false,
          customData: {
            iccid: 'iccid',
          },
        };
        await flushAsync();

        let networkName = listItem.$$('#networkName');
        assertTrue(!!networkName);
        assertEquals(itemName, networkName.textContent.trim());

        let subtitle = listItem.$$('#subtitle');
        assertTrue(!!subtitle);
        assertEquals(itemSubtitle, subtitle.textContent.trim());

        let spinner = listItem.$$('paper-spinner-lite');
        assertTrue(!!spinner);
      });

  test('Only active SIMs should show scanning subtext', async () => {
    const kTestIccid1 = '00000000000000000000';
    const kTestIccid2 = '11111111111111111111';
    const kTestEid = '124567890';
    const networkStateText = listItem.$$('#networkStateText');

    mojoApi_.setDeviceStateForTest({
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      simInfos: [
        {slot_id: 1, eid: kTestEid, iccid: kTestIccid1, isPrimary: false},
        {slot_id: 2, eid: '', iccid: kTestIccid2, isPrimary: true}
      ],
      scanning: true
    });
    const cellularNetwork1 = initCellularNetwork(
        'cellular_1_guid', 'cellular_1', kTestIccid1, kTestEid, '');
    const cellularNetwork2 = initCellularNetwork(
        'cellular_2_guid', 'cellular_2', kTestIccid2, '', '');

    // Assert that state text is hidden for inactive SIM.
    listItem.deviceState =
        mojoApi_.getDeviceStateForTest(mojom.NetworkType.kCellular);
    listItem.item = cellularNetwork1;
    await flushAsync();
    assertTrue(networkStateText.hidden);

    // Assert that scanning subtext is shown for active SIM.
    listItem.item = cellularNetwork2;
    await flushAsync();
    assertFalse(networkStateText.hidden);
    assertEquals(
        networkStateText.textContent.trim(),
        listItem.i18n('networkListItemScanning'));
  });
});
