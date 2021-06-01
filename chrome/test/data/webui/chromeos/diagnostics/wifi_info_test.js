// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';
import {fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

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
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#state'),
          `${fakeWifiNetwork.state}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#signalStrength'),
          `${fakeWifiNetwork.typeProperties.wifi.signalStrength}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#frequency'),
          `${fakeWifiNetwork.typeProperties.wifi.frequency}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#bssid'),
          fakeWifiNetwork.typeProperties.wifi.bssid);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#ssid'),
          fakeWifiNetwork.typeProperties.wifi.ssid);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#guid'),
          fakeWifiNetwork.guid);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#macAddress'),
          `${fakeWifiNetwork.macAddress}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#ipAddress'),
          `${fakeWifiNetwork.ipConfig.ipAddress}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#gateway'),
          `${fakeWifiNetwork.ipConfig.gateway}`);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#nameServers'),
          fakeWifiNetwork.ipConfig.nameServers[0]);
      dx_utils.assertTextContains(
          dx_utils.getDataPointValue(wifiInfoElement, '#subnetMask'),
          `${fakeWifiNetwork.ipConfig.routingPrefix}`);
    });
  });
}