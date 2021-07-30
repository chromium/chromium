// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkType, RoutineType} from 'chrome://diagnostics/diagnostics_types.js';
import {convertFrequencyToChannel, convertKibToGibDecimalString, getRoutinesByNetworkType, getSubnetMaskFromRoutingPrefix} from 'chrome://diagnostics/diagnostics_utils.js';

import {assertArrayEquals, assertEquals} from '../../chai_assert.js';

export function diagnosticsUtilsTestSuite() {
  test('ProperlyConvertsKibToGib', () => {
    assertEquals('0', convertKibToGibDecimalString(0, 0));
    assertEquals('0.00', convertKibToGibDecimalString(0, 2));
    assertEquals('0.000000', convertKibToGibDecimalString(0, 6));
    assertEquals('0', convertKibToGibDecimalString(1, 0));
    assertEquals('5.72', convertKibToGibDecimalString(6000000, 2));
    assertEquals('5.722046', convertKibToGibDecimalString(6000000, 6));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 + 1, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 - 1, 2));
    assertEquals('0.999999', convertKibToGibDecimalString(2 ** 20 - 1, 6));
  });

  test('ConvertRoutingPrefixToSubnetMask', () => {
    assertEquals(getSubnetMaskFromRoutingPrefix(1), '128.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(2), '192.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(3), '224.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(4), '240.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(5), '248.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(6), '252.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(7), '254.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(8), '255.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(9), '255.128.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(10), '255.192.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(11), '255.224.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(12), '255.240.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(13), '255.248.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(14), '255.252.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(15), '255.254.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(16), '255.255.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(17), '255.255.128.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(18), '255.255.192.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(19), '255.255.224.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(20), '255.255.240.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(21), '255.255.248.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(22), '255.255.252.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(23), '255.255.254.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(24), '255.255.255.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(25), '255.255.255.128');
    assertEquals(getSubnetMaskFromRoutingPrefix(26), '255.255.255.192');
    assertEquals(getSubnetMaskFromRoutingPrefix(27), '255.255.255.224');
    assertEquals(getSubnetMaskFromRoutingPrefix(28), '255.255.255.240');
    assertEquals(getSubnetMaskFromRoutingPrefix(29), '255.255.255.248');
    assertEquals(getSubnetMaskFromRoutingPrefix(30), '255.255.255.252');
    assertEquals(getSubnetMaskFromRoutingPrefix(31), '255.255.255.254');
    assertEquals(getSubnetMaskFromRoutingPrefix(32), '255.255.255.255');
  });

  test('ConvertFrequencyToChannel', () => {
    assertEquals(convertFrequencyToChannel(0), null);
    assertEquals(convertFrequencyToChannel(2411), null);
    // Calculates 2.4GHz channels.
    assertEquals(convertFrequencyToChannel(2412), 1);
    assertEquals(convertFrequencyToChannel(2417), 2);
    assertEquals(convertFrequencyToChannel(2422), 3);
    assertEquals(convertFrequencyToChannel(2427), 4);
    assertEquals(convertFrequencyToChannel(2432), 5);
    assertEquals(convertFrequencyToChannel(2437), 6);
    assertEquals(convertFrequencyToChannel(2442), 7);
    assertEquals(convertFrequencyToChannel(2447), 8);
    assertEquals(convertFrequencyToChannel(2452), 9);
    assertEquals(convertFrequencyToChannel(2457), 10);
    assertEquals(convertFrequencyToChannel(2462), 11);
    assertEquals(convertFrequencyToChannel(2467), 12);
    assertEquals(convertFrequencyToChannel(2472), 13);
    // Special 2.4GHz channel range for Japan
    assertEquals(convertFrequencyToChannel(2484), 14);
    assertEquals(convertFrequencyToChannel(2495), 14);
    // TODO(ashleydp): Fix expectation when 5GHz algorithm is ready.
    assertEquals(convertFrequencyToChannel(2496), null);
  });

  test('GetRoutinesByNetworkType', () => {
    /** @type {!Array<!RoutineType>} */
    const expectedRoutinesWifi = [
      RoutineType.kCaptivePortal,
      RoutineType.kDnsLatency,
      RoutineType.kDnsResolution,
      RoutineType.kDnsResolverPresent,
      RoutineType.kGatewayCanBePinged,
      RoutineType.kHttpFirewall,
      RoutineType.kHttpsFirewall,
      RoutineType.kHttpsLatency,
      RoutineType.kLanConnectivity,
      // assertArrayEquals wants values in order, code appends values to end
      // of array.
      RoutineType.kHasSecureWiFiConnection,
      RoutineType.kSignalStrength,
    ];

    /** @type {!Array<!RoutineType>} */
    const expectedRoutinesNotWifi = [
      RoutineType.kCaptivePortal,
      RoutineType.kDnsLatency,
      RoutineType.kDnsResolution,
      RoutineType.kDnsResolverPresent,
      RoutineType.kGatewayCanBePinged,
      RoutineType.kHttpFirewall,
      RoutineType.kHttpsFirewall,
      RoutineType.kHttpsLatency,
      RoutineType.kLanConnectivity,
    ];

    assertArrayEquals(
        expectedRoutinesWifi, getRoutinesByNetworkType(NetworkType.kWiFi));
    assertArrayEquals(
        expectedRoutinesNotWifi,
        getRoutinesByNetworkType(NetworkType.kEthernet));
    assertArrayEquals(
        expectedRoutinesNotWifi,
        getRoutinesByNetworkType(NetworkType.kCellular));
  });
}
