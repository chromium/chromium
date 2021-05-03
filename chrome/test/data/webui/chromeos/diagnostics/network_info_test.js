// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/network_info.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkInfoTestSuite() {
  /** @type {?NetworkInfoElement} */
  let networkInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkInfoElement.remove();
    networkInfoElement = null;
  });

  function initializeNetworkInfo() {
    assertFalse(!!networkInfoElement);

    // Add the network info to the DOM.
    networkInfoElement = /** @type {!NetworkInfoElement} */ (
        document.createElement('network-info'));
    assertTrue(!!networkInfoElement);
    document.body.appendChild(networkInfoElement);

    return flushTasks();
  }

  test('NetworkInfoPopulated', () => {
    return initializeNetworkInfo().then(() => {
      dx_utils.assertElementContainsText(
          networkInfoElement.$$('#cardTitle'), 'Network');
    });
  });

  test('WifiInfoPresent', () => {
    return initializeNetworkInfo().then(() => {
      const wifiInfoElement = networkInfoElement.$$('wifi-info');
      assertTrue(!!wifiInfoElement);
    });
  });

  test('EthernetInfoPresent', () => {
    return initializeNetworkInfo().then(() => {
      const ethernetInfoElement = networkInfoElement.$$('ethernet-info');
      assertTrue(!!ethernetInfoElement);
    });
  });

  test('CellularInfoPresent', () => {
    return initializeNetworkInfo().then(() => {
      const cellularInfoElement = networkInfoElement.$$('cellular-info');
      assertTrue(!!cellularInfoElement);
    });
  });
}