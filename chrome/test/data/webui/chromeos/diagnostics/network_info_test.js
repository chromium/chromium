// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeCellularNetwork, fakeEthernetNetwork, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {Network} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {NetworkInfoElement} from 'chrome://diagnostics/network_info.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkInfoTestSuite', function() {
  /** @type {?NetworkInfoElement} */
  let networkInfoElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    networkInfoElement.remove();
    networkInfoElement = null;
  });

  /**
   * @param {!Network} network
   */
  function initializeNetworkInfo(network) {
    assertFalse(!!networkInfoElement);

    // Add the network info to the DOM.
    networkInfoElement = /** @type {!NetworkInfoElement} */ (
        document.createElement('network-info'));
    assertTrue(!!networkInfoElement);
    networkInfoElement.network = network;
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  /**
   * @param {!Network} network
   * @return {!Promise}
   */
  function changeNetwork(network) {
    networkInfoElement.network = network;
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
