// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('SettingsTrafficCountersTest', function() {
  /**
   * Fake last reset time. Corresponds to a time on September 10, 2021.
   * @type {bigint}
   */
  const FAKE_TIME_IN_MICROSECONDS = BigInt(13275778457938000);
  /** @type {!mojoBase.mojom.Time} */
  const FAKE_INITIAL_LAST_RESET_TIME = {
    internalValue: FAKE_TIME_IN_MICROSECONDS,
  };
  /** @type {string} */
  const FAKE_INITIAL_FRIENDLY_DATE = 'September 10, 2021';
  /** @type {string} */
  const EXPECTED_INITIAL_DATA_USAGE_LABEL =
      'Data usage since September 10, 2021';
  /** @type {string} */
  const EXPECTED_INITIAL_DATA_USAGE_SUBLABEL = '1.80 KB';
  const EXPECTED_POST_RESET_FRIENDLY_DATE = 'September 12, 2021';
  /** @type {string} */
  const EXPECTED_POST_RESET_DATA_USAGE_LABEL =
      'Data usage since September 12, 2021';
  /** @type {string} */
  const EXPECTED_POST_RESET_DATA_USAGE_SUBLABEL = '0.00 B';

  /** @type {!SettingsTrafficCountersElement} */
  let settingsTrafficCounters;

  /** @type {?FakeNetworkConfig} */
  let networkConfigRemote_ = null;

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

  /**
   * @return {string}
   */
  function getDataUsageLabel() {
    const dataUsageLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector('#dataUsageLabel');
    assertTrue(!!dataUsageLabelDiv);
    return dataUsageLabelDiv.textContent.trim();
  }

  /**
   * @return {string}
   */
  function getDataUsageSubLabel() {
    const dataUsageSubLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector('#dataUsageSubLabel');
    assertTrue(!!dataUsageSubLabelDiv);
    return dataUsageSubLabelDiv.textContent.trim();
  }

  /**
   * @return {string}
   */
  function getResetDataUsageLabel() {
    const resetDataUsageSubLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector(
            '#resetDataUsageLabel');
    assertTrue(!!resetDataUsageSubLabelDiv);
    return resetDataUsageSubLabelDiv.textContent.trim();
  }

  /**
   * @return {!Object}
   */
  function getResetDataUsageButton() {
    const resetDataUsageButton =
        settingsTrafficCounters.shadowRoot.querySelector(
            '#resetDataUsageButton');
    assertTrue(!!resetDataUsageButton);
    return resetDataUsageButton;
  }

  /**
   * @return {string}
   */
  function getAutoDataUsageResetLabel() {
    const autoDataUsageResetLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector(
            '#autoDataUsageResetLabel');
    assertTrue(!!autoDataUsageResetLabelDiv);
    return autoDataUsageResetLabelDiv.textContent.trim();
  }

  /**
   * @return {string}
   */
  function getAutoDataUsageResetSubLabel() {
    const autoDataUsageResetSubLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector(
            '#autoDataUsageResetSubLabel');
    assertTrue(!!autoDataUsageResetSubLabelDiv);
    return autoDataUsageResetSubLabelDiv.textContent.trim();
  }

  /**
   * @return {string}
   */
  function getDaySelectionLabel() {
    const daySelectionLabelDiv =
        settingsTrafficCounters.shadowRoot.querySelector('#daySelectionLabel');
    assertTrue(!!daySelectionLabelDiv);
    return daySelectionLabelDiv.textContent.trim();
  }

  /**
   * @return {!Object}
   */
  function getAutoDataUsageResetToggle() {
    const autoDataUsageResetToggle =
        settingsTrafficCounters.shadowRoot.querySelector(
            '#autoDataUsageResetToggle');
    assertTrue(!!autoDataUsageResetToggle);
    return autoDataUsageResetToggle;
  }

  /**
   * @protected
   */
  function ensureDaySelectionInputIsNotPresent() {
    const daySelectionInput =
        settingsTrafficCounters.shadowRoot.querySelector('#daySelectionInput');
    assertFalse(!!daySelectionInput);
  }

  /**
   * @return {!Object}
   */
  function getDaySelectionInput() {
    const daySelectionInput =
        settingsTrafficCounters.shadowRoot.querySelector('#daySelectionInput');
    assertTrue(!!daySelectionInput);
    return daySelectionInput;
  }

  /**
   * @return {!Promise<!Object>}
   */
  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    networkConfigRemote_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigRemote_;

    settingsTrafficCounters =
        document.createElement('settings-traffic-counters');
    document.body.appendChild(settingsTrafficCounters);
    flush();

    // Remove default network.
    networkConfigRemote_.setNetworkConnectionStateForTest(
        'eth0_guid', ConnectionStateType.kNotConnected);
    flush();
  });

  test('Missing friendly date info', async function() {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;
    managedProperties.trafficCounterProperties.autoReset = true;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = 31;
    networkConfigRemote_.setManagedPropertiesForTest(managedProperties);
    await flushAsync();

    // Set traffic counters.
    networkConfigRemote_.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100, 100));
    await flushAsync();

    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushAsync();

    assertEquals(
        getDataUsageLabel(),
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageLastResetDateUnavailableLabel'));
    assertEquals(getDataUsageSubLabel(), EXPECTED_INITIAL_DATA_USAGE_SUBLABEL);
  });

  test('Show traffic counter info', async function() {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;
    managedProperties.trafficCounterProperties.lastResetTime =
        FAKE_INITIAL_LAST_RESET_TIME;
    managedProperties.trafficCounterProperties.friendlyDate =
        FAKE_INITIAL_FRIENDLY_DATE;
    managedProperties.trafficCounterProperties.autoReset = true;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = 31;
    networkConfigRemote_.setManagedPropertiesForTest(managedProperties);
    await flushAsync();

    // Set traffic counters.
    networkConfigRemote_.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100, 100));
    await flushAsync();

    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushAsync();

    assertEquals(getDataUsageLabel(), EXPECTED_INITIAL_DATA_USAGE_LABEL);
    assertEquals(getDataUsageSubLabel(), EXPECTED_INITIAL_DATA_USAGE_SUBLABEL);
    assertEquals(
        getResetDataUsageLabel(),
        settingsTrafficCounters.i18n('TrafficCountersDataUsageResetLabel'));
    assertEquals(
        getAutoDataUsageResetLabel(),
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageEnableAutoResetLabel'));
    assertEquals(
        getAutoDataUsageResetSubLabel(),
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageEnableAutoResetSublabel'));
    assertEquals(
        getDaySelectionLabel(),
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageAutoResetDayOfMonthLabel'));
    getAutoDataUsageResetToggle();
    assertEquals(getDaySelectionInput().value, 31);

    // Simulate a reset by updating the last reset time for the cellular
    // network. The internal value represents two days after the initial
    // reset time.
    networkConfigRemote_.setLastResetTimeForTest('cellular_guid', {
      internalValue: FAKE_INITIAL_LAST_RESET_TIME.internalValue +
          BigInt(2 * 24 * 60 * 60 * (1000 * 1000)),
    });
    networkConfigRemote_.setFriendlyDateForTest(
        'cellular_guid', EXPECTED_POST_RESET_FRIENDLY_DATE);
    await flushAsync();

    // Reset the data usage.
    getResetDataUsageButton().click();
    await flushAsync();

    assertEquals(getDataUsageLabel(), EXPECTED_POST_RESET_DATA_USAGE_LABEL);
    assertEquals(
        getDataUsageSubLabel(), EXPECTED_POST_RESET_DATA_USAGE_SUBLABEL);
    assertEquals(
        getResetDataUsageLabel(),
        settingsTrafficCounters.i18n('TrafficCountersDataUsageResetLabel'));
  });

  test('Enable traffic counters auto reset', async function() {
    // Disable auto reset initially.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;
    managedProperties.trafficCounterProperties.autoReset = false;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = null;
    managedProperties.trafficCounterProperties.lastResetTime =
        FAKE_INITIAL_LAST_RESET_TIME;
    managedProperties.trafficCounterProperties.friendlyDate =
        FAKE_INITIAL_FRIENDLY_DATE;
    networkConfigRemote_.setManagedPropertiesForTest(managedProperties);
    await flushAsync();

    // Set traffic counters.
    networkConfigRemote_.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100, 100));
    await flushAsync();

    // Load the settings traffic counters HTML for cellular_guid.
    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushAsync();
    ensureDaySelectionInputIsNotPresent();

    // Enable auto reset.
    getAutoDataUsageResetToggle().click();
    await flushAsync();

    // Verify that the correct day selection label is shown.
    assertEquals(
        getDaySelectionLabel(),
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageAutoResetDayOfMonthLabel'));
    // Verify that the correct user specified day is shown.
    assertEquals(getDaySelectionInput().value, 1);

    // Change the reset day to a valid value.
    getDaySelectionInput().value = 15;
    await flushAsync();
    assertEquals(getDaySelectionInput().value, 15);

    // Disable auto reset again.
    getAutoDataUsageResetToggle().click();
    await flushAsync();
    ensureDaySelectionInputIsNotPresent();
  });
});
