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

  function initProperties(properties) {
    assertTrue(!!properties.guid);
    mojoApi_.setManagedPropertiesForTest(properties);
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

    const cellular = OncMojo.getDefaultManagedProperties(
        chromeos.networkConfig.mojom.NetworkType.kCellular, 'cellular1_guid',
        '');
    cellular.name = OncMojo.createManagedString('cellular1');
    cellular.typeProperties.cellular.homeProvider = {name: 'Verizon Wireless'};
    cellular.typeProperties.cellular.eid = '10000';
    initProperties(cellular);

    listItem.item = OncMojo.getDefaultNetworkState(
        mojom.NetworkType.kCellular, 'cellular1');

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
});
