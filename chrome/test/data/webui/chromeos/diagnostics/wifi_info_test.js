// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';
import {fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

export function wifiInfoTestSuite() {
  /** @type {?WifiInfoElement} */
  let wifiInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    wifiInfoElement.remove();
    wifiInfoElement = null;
  });

  function initializeWifiInfo() {
    assertFalse(!!wifiInfoElement);

    // Add the wifi info to the DOM.
    wifiInfoElement =
        /** @type {!WifiInfoElement} */ (document.createElement('wifi-info'));
    assertTrue(!!wifiInfoElement);
    wifiInfoElement.network = fakeWifiNetwork;
    document.body.appendChild(wifiInfoElement);

    return flushTasks();
  }

  test('WifiInfoPopulated', () => {
    return initializeWifiInfo().then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#name'),
          `${fakeWifiNetwork.name}`);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#bssid'),
          fakeWifiNetwork.typeProperties.wifi.bssid);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#ssid'),
          fakeWifiNetwork.typeProperties.wifi.ssid);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#ipAddress'),
          `${fakeWifiNetwork.ipConfig.ipAddress}`);
    });
  });
}