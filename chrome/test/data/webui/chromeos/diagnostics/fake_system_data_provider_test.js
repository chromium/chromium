// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeBatteryInfo2, fakeCpuUsage, fakeMemoryUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';

suite('FakeSystemDataProviderTest', () => {
  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeSystemDataProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('GetSystemInfo', () => {
    /** @type {!DeviceCapabilities} */
    const capabilities = {
      has_battery: true,
    };

    /** @type {!VersionInfo} */
    const version = {
      milestone_version: 'M97',
    };

    /** @type {!SystemInfo} */
    const expected = {
      board_name: 'BestBoard',
      cpu_model: 'SuperFast CPU',
      total_memory_kib: 9999,
      cores_number: 4,
      version_info: version,
      device_capabilities: capabilities,
    };

    provider.setFakeSystemInfo(expected);
    return provider.getSystemInfo().then((systemInfo) => {
      assertDeepEquals(expected, systemInfo);
    });
  });

  test('GetBatteryInfo', () => {
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo().then((batteryInfo) => {
      assertDeepEquals(fakeBatteryInfo, batteryInfo);
    });
  });

  test('ObserveBatteryHealth', () => {
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    /** @type {!BatteryHealthObserver} */
    const batteryHealthObserverRemote = {
      onBatteryHealthUpdated: (batteryHealth) => {
        assertDeepEquals(fakeBatteryHealth[0], batteryHealth);
      }
    };

    return provider.observeBatteryHealth(batteryHealthObserverRemote);
  });

  test('ObserveBatteryChargeStatus', () => {
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    /** @type {!BatteryChargeStatusObserver} */
    const batteryChargeStatusObserverRemote = {
      onBatteryChargeStatusUpdated: (batteryChargeStatus) => {
        assertDeepEquals(fakeBatteryChargeStatus[0], batteryChargeStatus);
      }
    };

    return provider.observeBatteryChargeStatus(
        batteryChargeStatusObserverRemote);
  });

  test('ObserveCpuUsage', () => {
    provider.setFakeCpuUsage(fakeCpuUsage);

    /** @type {!CpuUsageObserver} */
    const cpuObserverRemote = {
      onCpuUsageUpdated: (cpuUsage) => {
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);
      }
    };

    return provider.observeCpuUsage(cpuObserverRemote);
  });

  test('ObserveMemoryUsage', () => {
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    /** @type {!MemoryUsageObserver} */
    const memoryUsageObserverRemote = {
      onMemoryUsageUpdated: (memoryUsage) => {
        assertDeepEquals(fakeMemoryUsage[0], memoryUsage);
      }
    };

    return provider.observeMemoryUsage(memoryUsageObserverRemote);
  });

  test('CallMethodWithNoValue', () => {
    // Don't set any fake data.
    return provider.getBatteryInfo().then((value) => {
      assertEquals(undefined, value);
    });
  });

  test('CallMethodDifferentDataAfterReset', () => {
    // Setup the fake data, but then reset it and set it with new data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    provider.reset();
    provider.setFakeBatteryInfo(fakeBatteryInfo2);

    return provider.getBatteryInfo().then((batteryInfo) => {
      assertDeepEquals(fakeBatteryInfo2, batteryInfo);
    });
  });

  test('CallMethodFirstThenDifferentDataAfterReset', () => {
    // Setup the initial fake data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo()
        .then((batteryInfo) => {
          assertDeepEquals(fakeBatteryInfo, batteryInfo);
        })
        .then(() => {
          // Reset and next time it should fire undefined.
          provider.reset();
          return provider.getBatteryInfo().then((value) => {
            assertEquals(undefined, value);
          });
        })
        .then(() => {
          // Set different data and next time should fire with it.
          provider.reset();
          provider.setFakeBatteryInfo(fakeBatteryInfo2);
          return provider.getBatteryInfo().then((batteryInfo) => {
            assertDeepEquals(fakeBatteryInfo2, batteryInfo);
          });
        });
  });
});