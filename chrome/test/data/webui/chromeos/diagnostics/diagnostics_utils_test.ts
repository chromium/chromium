// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {convertKibToGibDecimalString, getNetworkCardTitle, getRoutineGroups, getSignalStrength, getSubnetMaskFromRoutingPrefix, setDisplayStateInTitleForTesting} from 'chrome://diagnostics/diagnostics_utils.js';
import {NetworkType} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {RoutineType} from 'chrome://diagnostics/system_routine_controller.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';

suite('diagnosticsUtilsTestSuite', function() {
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
    const routineGroups: RoutineGroup[] = getRoutineGroups(NetworkType.kWiFi);
    const [
      localNetworkGroup,
       nameResolutionGroup,
       wifiGroup,
       internetConnectivityGroup,
      ]
      = routineGroups;

    // All groups should be present.
    assertEquals(routineGroups.length, 4);

    // WiFi group should exist and all three WiFi routines should be present.
    assert(wifiGroup);
    assertEquals(wifiGroup.routines.length, 3);
    assertEquals(wifiGroup.groupName, 'wifiGroupLabel');

    // ARC routines should be present in their categories.
    assert(nameResolutionGroup);
    assertTrue(
        nameResolutionGroup.routines.includes(RoutineType.kArcDnsResolution));
    assert(localNetworkGroup);
    assertTrue(localNetworkGroup.routines.includes(RoutineType.kArcPing));
    assert(internetConnectivityGroup);
    assertTrue(
        internetConnectivityGroup.routines.includes(RoutineType.kArcHttp));
  });

  test('NetworkTypeIsNotWiFi', () => {
    const routineGroups = getRoutineGroups(NetworkType.kEthernet);
    // WiFi group should be missing.
    assertEquals(routineGroups.length, 3);
    const groupNames = routineGroups.map(group => group.groupName);
    assertFalse(groupNames.includes('wifiGroupLabel'));
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

  test('GetSignalStrength', () => {
    assertEquals(getSignalStrength(0), '');
    assertEquals(getSignalStrength(1), '');
    assertEquals(getSignalStrength(14), 'Weak (14)');
    assertEquals(getSignalStrength(33), 'Average (33)');
    assertEquals(getSignalStrength(63), 'Good (63)');
    assertEquals(getSignalStrength(98), 'Excellent (98)');
  });
});
