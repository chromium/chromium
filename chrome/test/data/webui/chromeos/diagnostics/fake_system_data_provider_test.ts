// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeBatteryInfo2, fakeCpuUsage, fakeMemoryUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {BatteryChargeStatus, BatteryChargeStatusObserverRemote, BatteryHealth, BatteryHealthObserverRemote, BatteryInfo, CpuUsage, CpuUsageObserverRemote, DeviceCapabilities, MemoryUsage, MemoryUsageObserverRemote, SystemInfo, VersionInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeSystemDataProviderTestSuite', function() {
  let provider: FakeSystemDataProvider|null = null;

  setup(() => {
    provider = new FakeSystemDataProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('GetSystemInfo', () => {
    const capabilities: DeviceCapabilities = {
      hasBattery: true,
    };

    const version: VersionInfo = {
      milestoneVersion: 'M97',
      fullVersionString: 'M97.1234.5.6',
    };

    const expected: SystemInfo = {
      boardName: 'BestBoard',
      cpuModelName: 'SuperFast CPU',
      marketingName: 'Bestest 1000',
      totalMemoryKib: 9999,
      cpuThreadsCount: 4,
      cpuMaxClockSpeedKhz: 1000,
      versionInfo: version,
      deviceCapabilities: capabilities,
    };
    assert(provider);
    provider.setFakeSystemInfo(expected);
    return provider.getSystemInfo().then((result: {systemInfo: SystemInfo}) => {
      assertDeepEquals(expected, result.systemInfo);
    });
  });

  test('GetBatteryInfo', () => {
    assert(provider);
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo().then(
        (result: {batteryInfo: BatteryInfo}) => {
          assertDeepEquals(fakeBatteryInfo, result.batteryInfo);
        });
  });

  test('ObserveBatteryHealth', () => {
    assert(provider);
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    const batteryHealthObserverRemote = {
      onBatteryHealthUpdated: (batteryHealth: BatteryHealth) => {
        assertDeepEquals(fakeBatteryHealth[0], batteryHealth);
      },
    };

    provider.observeBatteryHealth(
        batteryHealthObserverRemote as BatteryHealthObserverRemote);
    return provider.getObserveBatteryHealthPromiseForTesting();
  });

  test('ObserveBatteryChargeStatus', () => {
    assert(provider);
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    const batteryChargeStatusObserverRemote = {
      onBatteryChargeStatusUpdated:
          (batteryChargeStatus: BatteryChargeStatus) => {
            assertDeepEquals(fakeBatteryChargeStatus[0], batteryChargeStatus);
          },
    };

    provider.observeBatteryChargeStatus(
        batteryChargeStatusObserverRemote as BatteryChargeStatusObserverRemote);
    return provider.getObserveBatteryChargeStatusPromiseForTesting();
  });

  test('ObserveCpuUsage', () => {
    assert(provider);
    provider.setFakeCpuUsage(fakeCpuUsage);

    const cpuObserverRemote = {
      onCpuUsageUpdated: (cpuUsage: CpuUsage) => {
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);
      },
    };

    provider.observeCpuUsage(cpuObserverRemote as CpuUsageObserverRemote);
    return provider.getObserveCpuUsagePromiseForTesting();
  });

  test('ObserveMemoryUsage', () => {
    assert(provider);
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    const memoryUsageObserverRemote = {
      onMemoryUsageUpdated: (memoryUsage: MemoryUsage) => {
        assertDeepEquals(fakeMemoryUsage[0], memoryUsage);
      },
    };

    provider.observeMemoryUsage(
        memoryUsageObserverRemote as MemoryUsageObserverRemote);
    return provider.getObserveMemoryUsagePromiseForTesting();
  });

  test('CallMethodDifferentDataAfterReset', () => {
    assert(provider);
    // Setup the fake data, but then reset it and set it with new data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    provider.reset();
    provider.setFakeBatteryInfo(fakeBatteryInfo2);

    return provider.getBatteryInfo().then(
        (result: {batteryInfo: BatteryInfo}) => {
          assertDeepEquals(fakeBatteryInfo2, result.batteryInfo);
        });
  });

  test('CallMethodFirstThenDifferentDataAfterReset', () => {
    // Setup the initial fake data.
    assert(provider);
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo()
        .then((result: {batteryInfo: BatteryInfo}) => {
          assertDeepEquals(fakeBatteryInfo, result.batteryInfo);
        })
        .then(() => {
          assert(provider);
          // Set different data and next time should fire with it.
          provider.reset();
          provider.setFakeBatteryInfo(fakeBatteryInfo2);
          return provider.getBatteryInfo().then(
              (result: {batteryInfo: BatteryInfo}) => {
                assertDeepEquals(fakeBatteryInfo2, result.batteryInfo);
              });
        });
  });

  test('ObserveCpuTwiceWithTrigger', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeCpuUsage.length >= 2);
    provider.setFakeCpuUsage(fakeCpuUsage);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const cpuObserverRemote = {
      onCpuUsageUpdated: (cpuUsage: CpuUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeCpuUsage[whichSample], cpuUsage);

        if (whichSample === 0) {
          firstResolver.resolve(null);
        } else {
          completeResolver.resolve(null);
        }
        whichSample++;
      },
    };

    provider.observeCpuUsage(cpuObserverRemote as CpuUsageObserverRemote);
    return provider.getObserveCpuUsagePromiseForTesting()
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          assert(provider);
          provider.triggerCpuUsageObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveMemoryTwiceWithTrigger', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeMemoryUsage.length >= 2);
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const memoryObserverRemote = {
      onMemoryUsageUpdated: (memoryUsage: MemoryUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeMemoryUsage[whichSample], memoryUsage);

        if (whichSample === 0) {
          firstResolver.resolve(null);
        } else {
          completeResolver.resolve(null);
        }
        whichSample++;
      },
    };

    provider.observeMemoryUsage(
        memoryObserverRemote as MemoryUsageObserverRemote);
    return provider.getObserveMemoryUsagePromiseForTesting()
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          assert(provider);
          provider.triggerMemoryUsageObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveBatteryHealthTwiceWithTrigger', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeBatteryHealth.length >= 2);
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const batteryHealthObserverRemote = {
      onBatteryHealthUpdated: (batteryHealth: BatteryHealth) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeBatteryHealth[whichSample], batteryHealth);

        if (whichSample === 0) {
          firstResolver.resolve(null);
        } else {
          completeResolver.resolve(null);
        }
        whichSample++;
      },
    };

    provider.observeBatteryHealth(
        batteryHealthObserverRemote as BatteryHealthObserverRemote);
    return provider.getObserveBatteryHealthPromiseForTesting()
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          assert(provider);
          provider.triggerBatteryHealthObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveBatteryChargeStatusTwiceWithTrigger', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeBatteryChargeStatus.length >= 2);
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const batteryChargeStatusObserverRemote = {
      onBatteryChargeStatusUpdated:
          (batteryChargeStatus: BatteryChargeStatus) => {
            // Only expect 2 calls.
            assertTrue(whichSample >= 0);
            assertTrue(whichSample <= 1);
            assertDeepEquals(
                fakeBatteryChargeStatus[whichSample], batteryChargeStatus);

            if (whichSample === 0) {
              firstResolver.resolve(null);
            } else {
              completeResolver.resolve(null);
            }
            whichSample++;
          },
    };

    provider.observeBatteryChargeStatus(
        batteryChargeStatusObserverRemote as BatteryChargeStatusObserverRemote);
    return provider.getObserveBatteryChargeStatusPromiseForTesting()
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          assert(provider);
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerBatteryChargeStatusObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveWithResetBetweenTriggers', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeCpuUsage.length >= 2);
    provider.setFakeCpuUsage(fakeCpuUsage);

    // Keep track of which call to the callback.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const cpuObserverRemote = {
      onCpuUsageUpdated: (cpuUsage: CpuUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);

        // In both cases we should get the first sample only.
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);

        if (whichSample === 0) {
          firstResolver.resolve(null);
        } else {
          completeResolver.resolve(null);
        }
        whichSample++;
      },
    };

    provider.observeCpuUsage(cpuObserverRemote as CpuUsageObserverRemote);
    return provider.getObserveCpuUsagePromiseForTesting()
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          assert(provider);
          // After the observer fires the first time, reset it and observe
          // again. We should go back to the initial sample.
          provider.reset();
          provider.setFakeCpuUsage(fakeCpuUsage);

          // Observing will implicitly trigger again.
          return provider.observeCpuUsage(
              cpuObserverRemote as CpuUsageObserverRemote);
        })
        .then(() => {
          // Wait for the second callback which should return the first sample
          // because it was reset.
          return completeResolver.promise;
        });
  });
});
