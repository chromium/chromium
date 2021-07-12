// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';
import {Network} from 'chrome://diagnostics/diagnostics_types.js';
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

  /*
   * @param {Network=}
   */
  function initializeWifiInfo(network = fakeWifiNetwork) {
    assertFalse(!!wifiInfoElement);

    // Add the wifi info to the DOM.
    wifiInfoElement =
        /** @type {!WifiInfoElement} */ (document.createElement('wifi-info'));
    assertTrue(!!wifiInfoElement);
    wifiInfoElement.network = network;
    document.body.appendChild(wifiInfoElement);

    return flushTasks();
  }

  test('WifiInfoPopulated', () => {
    const expectedGhz = 5.745;
    return initializeWifiInfo().then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#name'),
          `${fakeWifiNetwork.name}`);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#ipAddress'),
          `${fakeWifiNetwork.ipConfig.ipAddress}`);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#bssid'),
          fakeWifiNetwork.typeProperties.wifi.bssid);
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#signalStrength'),
          `${fakeWifiNetwork.typeProperties.wifi.signalStrength}`);
      // TODO(ashleydp): Update test expectation when 5 GHz channel conversion
      // algorithm provided.
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `? (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyConvertibleToChannel', () => {
    const networkOverride =
        /** @type {!Network} */ (Object.assign({}, fakeWifiNetwork));
    networkOverride.typeProperties.wifi.frequency = 2412;
    const expectedGhz = 2.412;
    return initializeWifiInfo().then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `1 (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyNotConvertibleToChannel', () => {
    const networkOverride =
        /** @type {!Network} */ (Object.assign({}, fakeWifiNetwork));
    // 2411 MHz is below 2.4 GHz frequency range.
    networkOverride.typeProperties.wifi.frequency = 2411;
    const expectedGhz = 2.411;
    return initializeWifiInfo().then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `? (${expectedGhz} GHz)`);
    });
  });
}
