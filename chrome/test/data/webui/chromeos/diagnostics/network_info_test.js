// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';

import {Network} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {setNetworkHealthProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkInfoTestSuite() {
  /** @type {?NetworkInfoElement} */
  let networkInfoElement = null;

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
    networkInfoElement.remove();
    networkInfoElement = null;
    provider.reset();
  });

  /**
   * @param {string} guid
   * @param {!Array<!Network>} networkStateList
   */
  function initializeNetworkInfo(guid, networkStateList) {
    assertFalse(!!networkInfoElement);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState(guid, networkStateList);

    // Add the network info to the DOM.
    networkInfoElement = /** @type {!NetworkInfoElement} */ (
        document.createElement('network-info'));
    assertTrue(!!networkInfoElement);
    networkInfoElement.guid = guid;
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  test('NetworkStatePopulated', () => {
    return initializeNetworkInfo('wifiGuid', [fakeWifiNetwork]).then(() => {
      dx_utils.assertElementContainsText(
          networkInfoElement.$$('#guid'), fakeWifiNetwork.guid);
    });
  });
}