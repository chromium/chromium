// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {WiFiNetwork, CellularNetwork, EthernetNetwork} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeCellularNetwork, fakeEthernetNetwork, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {NetworkInfoElement} from 'chrome://diagnostics/network_info.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkInfoTestSuite', function() {
  let networkInfoElement: NetworkInfoElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    networkInfoElement?.remove();
    networkInfoElement = null;
  });

  function initializeNetworkInfo(network: EthernetNetwork|WiFiNetwork|CellularNetwork): Promise<void> {
    assertFalse(!!networkInfoElement);

    // Add the network info to the DOM.
    networkInfoElement = document.createElement('network-info');
    assert(networkInfoElement);
    networkInfoElement.network = network as Network;
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  function changeNetwork(network: EthernetNetwork|WiFiNetwork|CellularNetwork): Promise<void> {
    assert(networkInfoElement);
    networkInfoElement.network = network as Network;
    return flushTasks();
  }

  test('CorrectInfoElementShown', () => {
    return initializeNetworkInfo(fakeWifiNetwork)
        .then(() => {
          // wifi-info should be visible.
          assertTrue(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
          return changeNetwork(fakeCellularNetwork);
        })
        .then(() => {
          // cellular-info should be visible.
          assertTrue(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));
          return changeNetwork(fakeEthernetNetwork);
        })
        .then(() => {
          // ethernet-info should be visible.
          assertTrue(
              isVisible(dx_utils.getEthernetInfoElement(networkInfoElement)));

          assertFalse(
              isVisible(dx_utils.getWifiInfoElement(networkInfoElement)));
          assertFalse(
              isVisible(dx_utils.getCellularInfoElement(networkInfoElement)));
        });
  });
});
