// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_list.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('NetworkListTest', function() {
  /** @type {!NetworkList|undefined} */
  let networkList;


  setup(function() {
    networkList = document.createElement('network-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    networkList.style.height = '100%';
    networkList.style.width = '100%';
    document.body.appendChild(networkList);
    flush();
  });

  test('focus() focuses the first item', function() {
    const testNetworks = [
      OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth0'),
      OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi'),
    ];
    networkList.networks = testNetworks;
    flush();

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
