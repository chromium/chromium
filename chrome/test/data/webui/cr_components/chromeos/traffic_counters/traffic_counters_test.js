// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://network/strings.m.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {TrafficCountersElement} from 'chrome://resources/ash/common/traffic_counters/traffic_counters.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';
import {FakeNetworkConfig} from '../../../chromeos/fake_network_config_mojom.js';

suite('TrafficCountersTest', function() {
  /** @type {!TrafficCountersElement} */
  let trafficCounters;

  /** @type {?FakeNetworkConfig} */
  let networkConfigRemote = null;

  /**
   * Fake last reset time. Corresponds to a time on September 10, 2021.
   * @type {bigint}
   */
  const FAKE_TIME_IN_MICROSECONDS = BigInt(13275778457938000);
  /** @type {!Time} */
  const FAKE_INITIAL_LAST_RESET_TIME = {
    internalValue: FAKE_TIME_IN_MICROSECONDS,
  };
  /**
   * @type {string} human readable string representing
   * the FAKE_INITIAL_LAST_RESET_TIME.
   */
  const FAKE_INITIAL_LAST_RESET_TIME_LOCALE_STRING = '9/10/2021, 2:14:17 PM';
  /**
   * Note that the hour here has been adjusted to test for locale.
   * @type {string} human readable string representing
   * the FAKE_POST_RESET_LAST_RESET_TIME_LOCALE_STRING.
   */
  const FAKE_POST_RESET_LAST_RESET_TIME_LOCALE_STRING = '9/10/2021, 1:14:18 PM';

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {number} rxBytes
   * @param {number} txBytes
   * @return {!Array<Object>} traffic counters
   */
  function generateTrafficCounters(rxBytes, txBytes) {
    return [
      {'source': 'Unknown', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Chrome', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'User', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Arc', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Crosvm', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Pluginvm', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Update Engine', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'Vpn', 'rxBytes': rxBytes, 'txBytes': txBytes},
      {'source': 'System', 'rxBytes': rxBytes, 'txBytes': txBytes},
    ];
  }

  /** @return {!NetworkHealthContainerElement} container element */
  function getContainer() {
    const container =
        trafficCounters.shadowRoot.querySelector('network-health-container');
    assertTrue(!!container);
    return /** @type {!NetworkHealthContainerElement} */ (container);
  }

  /** @return {string} label */
  function getContainerLabel() {
    return getContainer().label;
  }

  /**
   * @param {string} id
   * @return {Element|null} service div
   */
  function getServiceDiv(id) {
    const serviceDiv = getContainer().querySelector('#' + id);
    assertTrue(!!serviceDiv);
    return serviceDiv;
  }

  /**
   * @param {string} id
   * @return {string} label
   */
  function getLabelFor(id) {
    return getServiceDiv(id)
        .querySelector('.network-attribute-label')
        .textContent.trim();
  }

  /**
   * @param {string} id
   * @return {string} value
   */
  function getValueFor(id) {
    return getServiceDiv(id)
        .querySelector('.network-attribute-value')
        .textContent.trim();
  }

  /**
   * @param {!Array<Object>} expectedTrafficCounters
   * @return {boolean} whether expected and actual traffic counters match
   */
  function trafficCountersAreEqual(expectedTrafficCounters) {
    return JSON.stringify(JSON.parse(getValueFor('counters'))) ===
        JSON.stringify(expectedTrafficCounters);
  }

  /**
   * Compare the times ignoring locale (i.e., hour) differences.
   *
   * @param {string} actualTime
   * @param {string} expectedTime
   * @return {boolean} whether the times are equal
   */
  function compareTimeWithoutLocale(actualTime, expectedTime) {
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
    const indexActual = actual[1].indexOf(':');
    const indexExpected = expected[1].indexOf(':');
    return indexActual !== -1 && indexExpected !== -1 &&
        actualTime[1].substring(indexActual) ===
        expectedTime[1].substring(indexExpected);
  }

  /**
   * Disable type check here to work around FakeNetworkConfig type issues.
   * @suppress {checkTypes}
   * @param {!FakeNetworkConfig} networkConfig
   */
  function setMojoServiceRemote(networkConfig) {
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfig);
  }

  setup(function() {
    networkConfigRemote = new FakeNetworkConfig();
    setMojoServiceRemote(networkConfigRemote);

    trafficCounters = /** @type {!TrafficCountersElement} */ (
        document.createElement('traffic-counters'));
    document.body.appendChild(trafficCounters);
    flush();
  });

  test('Request and reset traffic counters', async function() {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;
    managedProperties.trafficCounterProperties.lastResetTime =
        FAKE_INITIAL_LAST_RESET_TIME;
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushAsync();

    // Set traffic counters.
    networkConfigRemote.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100, 100));
    // Remove default network.
    networkConfigRemote.setNetworkConnectionStateForTest(
        'eth0_guid', ConnectionStateType.kNotConnected);
    await flushAsync();

    // Requests traffic counters.
    trafficCounters.shadowRoot.querySelector('#requestButton').click();
    await flushAsync();

    // Verify network health container's label is "Cellular".
    assertEquals(getContainerLabel(), 'Cellular');
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
    await flushAsync();
    // Reset the traffic counters.
    getServiceDiv('reset').querySelector('#resetButton').click();
    await flushAsync();

    // Confirm values are correct post reset.
    // Verify network health container's label is "Cellular".
    assertEquals(getContainerLabel(), 'Cellular');
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
