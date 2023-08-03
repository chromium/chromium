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
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    const dataUsageLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector('#dataUsageLabel');
    assertTrue(!!dataUsageLabelDiv);
    return dataUsageLabelDiv.textContent!.trim();
  }

  function getDataUsageSubLabel(): string {
    const dataUsageSubLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector('#dataUsageSubLabel');
    assertTrue(!!dataUsageSubLabelDiv);
    return dataUsageSubLabelDiv.textContent!.trim();
  }

  function getResetDataUsageLabel(): string {
    const resetDataUsageSubLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector(
            '#resetDataUsageLabel');
    assertTrue(!!resetDataUsageSubLabelDiv);
    return resetDataUsageSubLabelDiv.textContent!.trim();
  }

  function getResetDataUsageButton(): HTMLButtonElement {
    const resetDataUsageButton =
        settingsTrafficCounters.shadowRoot!.querySelector<HTMLButtonElement>(
            '#resetDataUsageButton');
    assertTrue(!!resetDataUsageButton);
    return resetDataUsageButton;
  }

  function getAutoDataUsageResetLabel(): string {
    const autoDataUsageResetLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector(
            '#autoDataUsageResetLabel');
    assertTrue(!!autoDataUsageResetLabelDiv);
    return autoDataUsageResetLabelDiv.textContent!.trim();
  }

  function getAutoDataUsageResetSubLabel(): string {
    const autoDataUsageResetSubLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector(
            '#autoDataUsageResetSubLabel');
    assertTrue(!!autoDataUsageResetSubLabelDiv);
    return autoDataUsageResetSubLabelDiv.textContent!.trim();
  }

  function getDaySelectionLabel(): string {
    const daySelectionLabelDiv =
        settingsTrafficCounters.shadowRoot!.querySelector('#daySelectionLabel');
    assertTrue(!!daySelectionLabelDiv);
    return daySelectionLabelDiv.textContent!.trim();
  }

  function getAutoDataUsageResetToggle(): HTMLButtonElement {
    const autoDataUsageResetToggle =
        settingsTrafficCounters.shadowRoot!.querySelector<HTMLButtonElement>(
            '#autoDataUsageResetToggle');
    assertTrue(!!autoDataUsageResetToggle);
    return autoDataUsageResetToggle;
  }

  function ensureDaySelectionInputIsNotPresent(): void {
    const daySelectionInput =
        settingsTrafficCounters.shadowRoot!.querySelector('#daySelectionInput');
    assertNull(daySelectionInput);
  }

  function getDaySelectionInput(): HTMLInputElement {
    const daySelectionInput =
        settingsTrafficCounters.shadowRoot!.querySelector<HTMLInputElement>(
            '#daySelectionInput');
    assertTrue(!!daySelectionInput);
    return daySelectionInput;
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
    managedProperties.trafficCounterProperties.autoReset = true;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = 31;
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
    managedProperties.trafficCounterProperties.lastResetTime =
        FAKE_INITIAL_LAST_RESET_TIME;
    managedProperties.trafficCounterProperties.friendlyDate =
        FAKE_INITIAL_FRIENDLY_DATE;
    managedProperties.trafficCounterProperties.autoReset = true;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = 31;
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
    assertEquals(
        settingsTrafficCounters.i18n('TrafficCountersDataUsageResetLabel'),
        getResetDataUsageLabel());
    assertEquals(
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageEnableAutoResetLabel'),
        getAutoDataUsageResetLabel());
    assertEquals(
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageEnableAutoResetSublabel'),
        getAutoDataUsageResetSubLabel());
    assertEquals(
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageAutoResetDayOfMonthLabel'),
        getDaySelectionLabel());
    getAutoDataUsageResetToggle();
    assertEquals(31, getDaySelectionInput().value);

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
    getResetDataUsageButton().click();
    await flushTasks();

    assertEquals(EXPECTED_POST_RESET_DATA_USAGE_LABEL, getDataUsageLabel());
    assertEquals(
        EXPECTED_POST_RESET_DATA_USAGE_SUBLABEL, getDataUsageSubLabel());
    assertEquals(
        settingsTrafficCounters.i18n('TrafficCountersDataUsageResetLabel'),
        getResetDataUsageLabel());
  });

  test('Enable traffic counters auto reset', async () => {
    // Disable auto reset initially.
    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    managedProperties.connectionState = ConnectionStateType.kConnected;
    managedProperties.connectable = true;
    managedProperties.trafficCounterProperties.autoReset = false;
    managedProperties.trafficCounterProperties.userSpecifiedResetDay = 0;
    managedProperties.trafficCounterProperties.lastResetTime =
        FAKE_INITIAL_LAST_RESET_TIME;
    managedProperties.trafficCounterProperties.friendlyDate =
        FAKE_INITIAL_FRIENDLY_DATE;
    networkConfigRemote.setManagedPropertiesForTest(managedProperties);
    await flushTasks();

    // Set traffic counters.
    // A bigint primitive is created by appending n to the end of an integer
    // literal
    networkConfigRemote.setTrafficCountersForTest(
        'cellular_guid', generateTrafficCounters(100n, 100n));
    await flushTasks();

    // Load the settings traffic counters HTML for cellular_guid.
    settingsTrafficCounters.guid = 'cellular_guid';
    settingsTrafficCounters.load();
    await flushTasks();
    ensureDaySelectionInputIsNotPresent();

    // Enable auto reset.
    getAutoDataUsageResetToggle().click();
    await flushTasks();

    // Verify that the correct day selection label is shown.
    assertEquals(
        settingsTrafficCounters.i18n(
            'TrafficCountersDataUsageAutoResetDayOfMonthLabel'),
        getDaySelectionLabel());
    // Verify that the correct user specified day is shown.
    assertEquals(1, getDaySelectionInput().value);

    // Change the reset day to a valid value.
    getDaySelectionInput().value = '15';
    await flushTasks();
    assertEquals('15', getDaySelectionInput().value);

    // Disable auto reset again.
    getAutoDataUsageResetToggle().click();
    await flushTasks();
    ensureDaySelectionInputIsNotPresent();
  });
});
