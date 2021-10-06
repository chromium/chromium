// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkType, RoutineType} from 'chrome://diagnostics/diagnostics_types.js';
import {convertKibToGibDecimalString, getNetworkCardTitle, getRoutineGroups, getSubnetMaskFromRoutingPrefix, setDisplayStateInTitleForTesting} from 'chrome://diagnostics/diagnostics_utils.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

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
    // '0' indicates an unset value.
    assertEquals(getSubnetMaskFromRoutingPrefix(0), '');
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

  test('AllRoutineGroupsPresent', () => {
    loadTimeData.overrideValues({enableArcNetworkDiagnostics: true});
    let isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    let routineGroups = getRoutineGroups(NetworkType.kWiFi, isArcEnabled);

    // All groups should be present.
    assertEquals(routineGroups.length, 4);

    // WiFi group should exist and all three WiFi routines should be present.
    let wifiGroup = routineGroups[2];
    assertEquals(wifiGroup.routines.length, 3);
    assertEquals(wifiGroup.groupName, 'wifiGroupLabel');

    // ARC routines should be present in their categories.
    let nameResolutionGroup = routineGroups[1];
    assertTrue(
        nameResolutionGroup.routines.includes(RoutineType.kArcDnsResolution));

    let internetConnectivityGroup = routineGroups[3];
    assertTrue(
        internetConnectivityGroup.routines.includes(RoutineType.kArcPing));
    assertTrue(
        internetConnectivityGroup.routines.includes(RoutineType.kArcHttp));
  });

  test('NetworkTypeIsNotWiFi', () => {
    let isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    let routineGroups = getRoutineGroups(NetworkType.kEthernet, isArcEnabled);
    // WiFi group should be missing.
    assertEquals(routineGroups.length, 3);
    let groupNames = routineGroups.map(group => group.groupName);
    assertFalse(groupNames.includes('wifiGroupLabel'));
  });

  test('ArcRoutinesDisabled', () => {
    loadTimeData.overrideValues({enableArcNetworkDiagnostics: false});
    let isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    let routineGroups = getRoutineGroups(NetworkType.kEthernet, isArcEnabled);

    let nameResolutionGroup = routineGroups[1];
    assertFalse(
        nameResolutionGroup.routines.includes(RoutineType.kArcDnsResolution));

    let internetConnectivityGroup = routineGroups[2];
    assertFalse(
        internetConnectivityGroup.routines.includes(RoutineType.kArcPing));
    assertFalse(
        internetConnectivityGroup.routines.includes(RoutineType.kArcHttp));
  });

  test('GetNetworkCardTitle', () => {
    // Force connection state into title by setting displayStateInTitle to true.
    setDisplayStateInTitleForTesting(true);
    assertEquals(
        'Ethernet (Online)', getNetworkCardTitle('Ethernet', 'Online'));

    // Default state is to not display connection details in title.
    setDisplayStateInTitleForTesting(false);
    assertEquals('Ethernet', getNetworkCardTitle('Ethernet', 'Online'));
  });
}
