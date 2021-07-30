// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/wifi_info.js';
import {Network, WiFiStateProperties} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeDisconnectedWifiNetwork, fakeWifiNetwork, fakeWiFiStateProperties} from 'chrome://diagnostics/fake_data.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {assertDataPointHasExpectedHeaderAndValue, assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

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

  /**
   * Helper function provides WiFi network with overridden typeProperties wifi
   * value.
   * @param {!WiFiStateProperties} stateProperties
   * @return {!Network}
   */
  function getWifiNetworkWithWiFiStatePropertiesOf(stateProperties) {
    const wifiTypeProperties =
        Object.assign({}, fakeWiFiStateProperties, stateProperties);
    return /** @type {!Network} */ (Object.assign({}, fakeWifiNetwork, {
      typeProperties: {
        wifi: wifiTypeProperties,
      }
    }));
  }

  test('WifiInfoPopulated', () => {
    const expectedGhz = 5.745;
    return initializeWifiInfo().then(() => {
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#ssid', wifiInfoElement.i18n('networkSsidLabel'),
          `${fakeWifiNetwork.typeProperties.wifi.ssid}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#ipAddress',
          wifiInfoElement.i18n('networkIpAddressLabel'),
          `${fakeWifiNetwork.ipConfig.ipAddress}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#bssid', wifiInfoElement.i18n('networkBssidLabel'),
          `${fakeWifiNetwork.typeProperties.wifi.bssid}`);
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security',
          wifiInfoElement.i18n('networkSecurityLabel'), '');
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#signalStrength',
          wifiInfoElement.i18n('networkSignalStrengthLabel'),
          `${fakeWifiNetwork.typeProperties.wifi.signalStrength}`);
      // TODO(ashleydp): Update test expectation when 5 GHz channel conversion
      // algorithm provided.
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#channel',
          wifiInfoElement.i18n('networkChannelLabel'),
          `? (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyConvertibleToChannel', () => {
    // 2412 is the minimum 2.4GHz frequency which can be converted into a valid
    // channel.
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 2412}));
    const expectedGhz = 2.412;
    return initializeWifiInfo(testNetwork).then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `1 (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyNotConvertibleToChannel', () => {
    // 2411 is below the minimum 2.4GHz frequency and cannot be converted into
    // a valid channel.
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 2411}));
    const expectedGhz = 2.411;
    return initializeWifiInfo(testNetwork).then(() => {
      assertTextContains(
          getDataPointValue(wifiInfoElement, '#channel'),
          `? (${expectedGhz} GHz)`);
    });
  });

  test('FrequencyZeroDisplaysEmptyString', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({frequency: 0}));
    return initializeWifiInfo(testNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#channel'), '');
    });
  });

  test('FrequencyUndefinedDisplaysEmptyString', () => {
    return initializeWifiInfo(fakeDisconnectedWifiNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#channel'), '');
    });
  });

  test('WiFiInfoSecurityBasedOnNetwork', () => {
    return initializeWifiInfo().then(() => {
      const expectedHeader = wifiInfoElement.i18n('networkSecurityLabel');
      // TODO(ashleydp): Update test when security data provided.
      assertDataPointHasExpectedHeaderAndValue(
          wifiInfoElement, '#security', expectedHeader, '');
    });
  });

  test('SignalStrengthZeroDisplaysEmptyString', () => {
    const testNetwork = getWifiNetworkWithWiFiStatePropertiesOf(
        /** @type {!WiFiStateProperties} */ ({signalStrength: 0}));
    return initializeWifiInfo(testNetwork).then(() => {
      assertEquals(getDataPointValue(wifiInfoElement, '#signalStrength'), '');
    });
  });
}
