// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsTrafficCountersElement} from 'chrome://os-settings/lazy_load.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {TrafficCounter, TrafficCounterSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-traffic-counters>', () => {
  /**
   * Fake last reset time. Corresponds to a time on September 10, 2021.
   */
  const FAKE_TIME_IN_MICROSECONDS: bigint = BigInt(13275778457938000);
  const FAKE_INITIAL_LAST_RESET_TIME: Time = {
    internalValue: FAKE_TIME_IN_MICROSECONDS,
  };
  const FAKE_INITIAL_FRIENDLY_DATE = 'September 10, 2021';
  const EXPECTED_INITIAL_DATA_USAGE_LABEL =
      'Data usage since September 10, 2021';
  const EXPECTED_INITIAL_DATA_USAGE_SUBLABEL = '1.80 KB';
  const EXPECTED_POST_RESET_FRIENDLY_DATE = 'September 12, 2021';
  const EXPECTED_POST_RESET_DATA_USAGE_LABEL =
      'Data usage since September 12, 2021';
  const EXPECTED_POST_RESET_DATA_USAGE_SUBLABEL = '0.00 B';

  let settingsTrafficCounters: SettingsTrafficCountersElement;
  let networkConfigRemote: FakeNetworkConfig;

  function generateTrafficCounters(
      rxBytes: bigint, txBytes: bigint): TrafficCounter[] {
    return [
      {
        source: TrafficCounterSource.kUnknown,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kChrome,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kUser,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kArc,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kCrosvm,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kPluginvm,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kUpdateEngine,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kVpn,
        rxBytes,
        txBytes,
      },
      {
        source: TrafficCounterSource.kSystem,
        rxBytes,
        txBytes,
      },
    ];
  }

  function getDataUsageLabel(): string {
    const dataUsageLabelDiv = settingsTrafficCounters.$.dataUsageLabel;
    return dataUsageLabelDiv.textContent!.trim();
  }

  function getDataUsageSubLabel(): string {
    const dataUsageSubLabelDiv = settingsTrafficCounters.$.dataUsageSubLabel;
    return dataUsageSubLabelDiv.textContent!.trim();
  }

  setup(() => {
    networkConfigRemote = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigRemote);

    settingsTrafficCounters =
        document.createElement('settings-traffic-counters');
    document.body.appendChild(settingsTrafficCounters);
    flush();

    // Remove default network.
    networkConfigRemote.setNetworkConnectionStateForTest(
        'eth0_guid', ConnectionStateType.kNotConnected);
    flush();
  });

  teardown(() => {
    settingsTrafficCounters.remove();
  });

  test('Missing friendly date info', async () => {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;

    const trafficCounterProps = OncMojo.createTrafficCounterProperties();
    trafficCounterProps.userSpecifiedResetDay = 31;
    managedProperties.trafficCounterProperties = trafficCounterProps;
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushTasks();

    // Set traffic counters.
    // A bigint primitive is created by appending n to the end of an integer
    // literal
    networkConfigRemote.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100n, 100n));
    await flushTasks();

    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushTasks();

    assertEquals(
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageLastResetDateUnavailableLabel'),
        getDataUsageLabel());
    assertEquals(EXPECTED_INITIAL_DATA_USAGE_SUBLABEL, getDataUsageSubLabel());
  });

  test('Show traffic counter info', async () => {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;

    const trafficCounterProps = OncMojo.createTrafficCounterProperties();
    trafficCounterProps.lastResetTime = FAKE_INITIAL_LAST_RESET_TIME;
    trafficCounterProps.friendlyDate = FAKE_INITIAL_FRIENDLY_DATE;
    trafficCounterProps.userSpecifiedResetDay = 31;
    managedProperties.trafficCounterProperties = trafficCounterProps;
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushTasks();

    // Set traffic counters.
    // A bigint primitive is created by appending n to the end of an integer
    // literal
    networkConfigRemote.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100n, 100n));
    await flushTasks();

    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushTasks();

    assertEquals(EXPECTED_INITIAL_DATA_USAGE_LABEL, getDataUsageLabel());
    assertEquals(EXPECTED_INITIAL_DATA_USAGE_SUBLABEL, getDataUsageSubLabel());
    assertEquals(String(31), settingsTrafficCounters.$.resetDayList.value);

    // Simulate a reset by updating the last reset time for the cellular
    // network. The internal value represents two days after the initial
    // reset time.
    networkConfigRemote.setLastResetTimeForTest('cellular_guid', {
      internalValue: FAKE_INITIAL_LAST_RESET_TIME.internalValue +
          BigInt(2 * 24 * 60 * 60 * (1000 * 1000)),
    });
    networkConfigRemote.setFriendlyDateForTest(
        'cellular_guid', EXPECTED_POST_RESET_FRIENDLY_DATE);
    await flushTasks();

    // Reset the data usage.
    settingsTrafficCounters.$.resetDataUsageButton.click();
    await flushTasks();

    assertEquals(EXPECTED_POST_RESET_DATA_USAGE_LABEL, getDataUsageLabel());
    assertEquals(
        EXPECTED_POST_RESET_DATA_USAGE_SUBLABEL, getDataUsageSubLabel());
  });

  test('Reset date functionality', async () => {
    // Set managed properties for a connected cellular network.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;

    const trafficCounterProps = OncMojo.createTrafficCounterProperties();
    trafficCounterProps.userSpecifiedResetDay = 31;
    managedProperties.trafficCounterProperties = trafficCounterProps;
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushTasks();

    settingsTrafficCounters.guid = 'cellular_guid';
    await flushTasks();

    assertEquals('31', settingsTrafficCounters.$.resetDayList.value);

    // Simulate an auto reset day update.
    settingsTrafficCounters.$.resetDayList.value = '5';
    settingsTrafficCounters.$.resetDayList.dispatchEvent(
        new CustomEvent('change'));
    await flushTasks();

    await networkConfigRemote.whenCalled('setTrafficCountersResetDay');

    const properties = await networkConfigRemote.getManagedProperties(
        settingsTrafficCounters.guid);

    const trafficCounterPropsRet = properties.result.trafficCounterProperties;
    assertTrue(trafficCounterPropsRet !== undefined);
    assertEquals(5, trafficCounterPropsRet!.userSpecifiedResetDay);
  });
});
