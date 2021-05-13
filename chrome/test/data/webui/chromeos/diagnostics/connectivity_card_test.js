// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/connectivity_card.js';

import {Network} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeEthernetNetwork, fakeNetworkGuidInfoList} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function connectivityCardTestSuite() {
  /** @type {?ConnectivityCardElement} */
  let connectivityCardElement = null;

  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    connectivityCardElement.remove();
    connectivityCardElement = null;
    provider.reset();
  });

  /**
   * @param {string} activeGuid
   * @param {!Array<!Network>} networkStateList
   */
  function initializeConnectivityCard(activeGuid, networkStateList) {
    assertFalse(!!connectivityCardElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState(activeGuid, networkStateList);

    // Add the connectivity card to the DOM.
    connectivityCardElement = /** @type {!ConnectivityCardElement} */ (
        document.createElement('connectivity-card'));
    assertTrue(!!connectivityCardElement);
    connectivityCardElement.activeGuid = activeGuid;
    document.body.appendChild(connectivityCardElement);

    return flushTasks();
  }

  test('ConnectivityCardPopulated', () => {
    return initializeConnectivityCard('ethernetGuid', [fakeEthernetNetwork])
        .then(() => {
          const ethernetInfoElement = dx_utils.getEthernetInfoElement(
              connectivityCardElement.$$('network-info'));
          dx_utils.assertTextContains(
              dx_utils.getDataPointValue(ethernetInfoElement, '#guid'),
              fakeEthernetNetwork.guid);
        });
  });
}