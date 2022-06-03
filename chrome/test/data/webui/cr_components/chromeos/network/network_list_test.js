// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_list.m.js';

// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

suite('NetworkListTest', function() {
  /** @type {!NetworkList|undefined} */
  let networkList;

  let mojom;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;

    networkList = document.createElement('network-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    networkList.style.height = '100%';
    networkList.style.width = '100%';
    document.body.appendChild(networkList);
    Polymer.dom.flush();
  });

  test('focus() focuses the first item', function() {
    const testNetworks = [
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi'),
    ];
    networkList.networks = testNetworks;
    Polymer.dom.flush();

    // No items are focused initially.
    const items = networkList.shadowRoot.querySelectorAll('network-list-item');
    const firstItem = items[0];
    let activeElement = getDeepActiveElement();
    assertNotEquals(activeElement, firstItem);

    // Focus the top-level list; the first item is focused.
    networkList.focus();
    activeElement = getDeepActiveElement();
    assertEquals(activeElement, firstItem);

    // Now focus the second item to show that focusing the list focuses the
    // first element independent of previous focus.
    const secondItem = items[1];
    secondItem.focus();
    activeElement = getDeepActiveElement();
    assertNotEquals(activeElement, firstItem);

    networkList.focus();
    activeElement = getDeepActiveElement();
    assertEquals(activeElement, firstItem);
  });
});
