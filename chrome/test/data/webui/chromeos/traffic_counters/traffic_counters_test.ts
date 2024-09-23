// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://network/strings.m.js';
import 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {NetworkHealthContainerElement} from 'chrome://resources/ash/common/network_health/network_health_container.js';
import type {TrafficCountersElement} from 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

suite('TrafficCountersTest', function() {
  let trafficCounters: TrafficCountersElement;

  let networkConfigRemote: FakeNetworkConfig;

  /**
   * Fake last reset time. Corresponds to a time on September 10, 2021.
   */
  const FAKE_TIME_IN_MICROSECONDS = BigInt(13275778457938000);
  const FAKE_INITIAL_LAST_RESET_TIME = {
    internalValue: FAKE_TIME_IN_MICROSECONDS,
  };
  /**
   * human readable string representing
   * the FAKE_INITIAL_LAST_RESET_TIME.
   */
  const FAKE_INITIAL_LAST_RESET_TIME_LOCALE_STRING = '9/10/2021, 2:14:17 PM';
  /**
   * Note that the hour here has been adjusted to test for locale.
   * human readable string representing
   * the FAKE_POST_RESET_LAST_RESET_TIME_LOCALE_STRING.
   */
  const FAKE_POST_RESET_LAST_RESET_TIME_LOCALE_STRING = '9/10/2021, 1:14:18 PM';

  function generateTrafficCounters(rxBytes: number, txBytes: number): Object[] {
    return [
      {
        'source': 'Unknown',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Chrome',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'User',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Arc',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Crosvm',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Pluginvm',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Update Engine',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'Vpn',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
      {
        'source': 'System',
        'rxBytes': rxBytes,
        'txBytes': txBytes,
      },
    ];
  }

  function getContainer() {
    const container = trafficCounters.shadowRoot!
                          .querySelector<NetworkHealthContainerElement>(
                              'network-health-container');
    assertTrue(!!container);
    return (container);
  }

  function getServiceDiv(id: string) {
    const serviceDiv = getContainer()!.querySelector('#' + id);
    assertTrue(!!serviceDiv);
    return serviceDiv;
  }

  function getLabelFor(id: string): string {
    return getServiceDiv(id)!.querySelector('.network-attribute-label')!
        .textContent!.trim();
  }

  function getValueFor(id: string) {
    return getServiceDiv(id)!.querySelector('.network-attribute-value')!
        .textContent!.trim();
  }

  function trafficCountersAreEqual(expectedTrafficCounters: Object[]): boolean {
    return JSON.stringify(JSON.parse(getValueFor('counters'))) ===
        JSON.stringify(expectedTrafficCounters);
  }

  /**
   * Compare the times ignoring locale (i.e., hour) differences.
   */
  function compareTimeWithoutLocale(
      actualTime: string, expectedTime: string): boolean {
    const actual = actualTime.split(', ');
    const expected = expectedTime.split(', ');
    if (actual.length !== 2 && expected.length !== 2) {
      return false;
    }
    // Check date portion.
    if (actual[0] !== expected[0]) {
      return false;
    }
    // Ignore the value before the first ":", which represents the hour.
    const indexActual = actual[1]!.indexOf(':');
    const indexExpected = expected[1]!.indexOf(':');
    return indexActual !== -1 && indexExpected !== -1 &&
        actualTime[1]!.substring(indexActual) ===
        expectedTime[1]!.substring(indexExpected);
  }

  /**
   * Disable type check here to work around FakeNetworkConfig type issues.
   */
  function setMojoServiceRemote(networkConfig: FakeNetworkConfig) {
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfig);
  }

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();
    setMojoServiceRemote(networkConfigRemote);

    trafficCounters = (document.createElement('traffic-counters'));
    document.body.appendChild(trafficCounters);
    flushTasks();

    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;

    // Define trafficCounterProps as const and modify its properties
    const trafficCounterProps = OncMojo.createTrafficCounterProperties();
    trafficCounterProps.lastResetTime = FAKE_INITIAL_LAST_RESET_TIME;
    managedProperties.trafficCounterProperties = trafficCounterProps;

    // Set the managed properties for the test
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushTasks();

    // Set traffic counters.
    networkConfigRemote.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100, 100));
    // Remove default network.
    networkConfigRemote.setNetworkConnectionStateForTest(
        'eth0_guid', ConnectionStateType.kNotConnected);
    await flushTasks();

    // Requests traffic counters.

    trafficCounters.shadowRoot!.querySelector<HTMLElement>(
                                   '#requestButton')!.click();
    await flushTasks();
  });

  test('Click and check if the network row has expanded', async function() {
    assertFalse(getContainer()!.expanded);
    getContainer()!.dispatchEvent(new CustomEvent('toggle-expanded', {
      bubbles: true,
      composed: true,
    }));
    await flushTasks();
    assertTrue(getContainer()!.expanded);
  });

  test('Request and reset traffic counters', async function() {
    // Verify service name is "cellular".
    assertEquals(getLabelFor('name'), trafficCounters.i18n('OncName'));
    assertEquals(getValueFor('name'), 'cellular');
    // Verify service GUID is "cellular_guid".
    assertEquals(
        getLabelFor('guid'), trafficCounters.i18n('TrafficCountersGuid'));
    assertEquals(getValueFor('guid'), 'cellular_guid');
    // Verify correct traffic counters.
    assertEquals(
        getLabelFor('counters'),
        trafficCounters.i18n('TrafficCountersTrafficCounters'));
    assertTrue(trafficCountersAreEqual(generateTrafficCounters(100, 100)));
    // Verify correct last reset time.
    assertEquals(
        getLabelFor('time'),
        trafficCounters.i18n('TrafficCountersLastResetTime'));
    assertTrue(compareTimeWithoutLocale(
        getValueFor('time'), FAKE_INITIAL_LAST_RESET_TIME_LOCALE_STRING));
    // Verify reset traffic counters button exists.
    getServiceDiv('reset');

    // Simulate a reset by updating the last reset time for the cellular
    // network. The internal value represents one second.
    networkConfigRemote.setLastResetTimeForTest('cellular_guid', {
      internalValue:
          FAKE_INITIAL_LAST_RESET_TIME.internalValue + BigInt(1000 * 1000),
    });
    await flushTasks();
    // Reset the traffic counters.
    getServiceDiv('reset')!.querySelector<HTMLElement>('#resetButton')!.click();
    await flushTasks();

    // Confirm values are correct post reset.
    // Verify service name is "cellular".
    assertEquals(getLabelFor('name'), trafficCounters.i18n('OncName'));
    assertEquals(getValueFor('name'), 'cellular');
    // Verify service GUID is "cellular_guid".
    assertEquals(
        getLabelFor('guid'), trafficCounters.i18n('TrafficCountersGuid'));
    assertEquals(getValueFor('guid'), 'cellular_guid');
    // Verify correct traffic counters.
    assertEquals(
        getLabelFor('counters'),
        trafficCounters.i18n('TrafficCountersTrafficCounters'));
    assertTrue(trafficCountersAreEqual(generateTrafficCounters(0, 0)));
    // Verify correct last reset time.
    assertEquals(
        getLabelFor('time'),
        trafficCounters.i18n('TrafficCountersLastResetTime'));
    assertTrue(compareTimeWithoutLocale(
        getValueFor('time'), FAKE_POST_RESET_LAST_RESET_TIME_LOCALE_STRING));
    // Verify reset traffic counters button exists.
    getServiceDiv('reset');
  });
});
