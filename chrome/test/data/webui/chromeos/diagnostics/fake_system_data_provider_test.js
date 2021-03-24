// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatusObserver, BatteryHealthObserver, CpuUsageObserver, DeviceCapabilities, MemoryUsageObserver, SystemInfo, VersionInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeBatteryInfo2, fakeCpuUsage, fakeMemoryUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function fakeSystemDataProviderTestSuite() {
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
      hasBattery: true,
    };

    /** @type {!VersionInfo} */
    const version = {
      milestoneVersion: 'M97',
      fullVersionString: 'M97.1234.5.6',
    };

    /** @type {!SystemInfo} */
    const expected = {
      boardName: 'BestBoard',
      cpuModelName: 'SuperFast CPU',
      marketingName: 'Bestest 1000',
      totalMemoryKib: 9999,
      cpuThreadsCount: 4,
      cpuMaxClockSpeedKhz: 1000,
      versionInfo: version,
      deviceCapabilities: capabilities,
    };

    provider.setFakeSystemInfo(expected);
    return provider.getSystemInfo().then((result) => {
      assertDeepEquals(expected, result.systemInfo);
    });
  });

  test('GetBatteryInfo', () => {
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo().then((result) => {
      assertDeepEquals(fakeBatteryInfo, result.batteryInfo);
    });
  });

  test('ObserveBatteryHealth', () => {
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    const batteryHealthObserverRemote = /** @type {!BatteryHealthObserver} */ ({
      onBatteryHealthUpdated: (batteryHealth) => {
        assertDeepEquals(fakeBatteryHealth[0], batteryHealth);
      }
    });

    return provider.observeBatteryHealth(batteryHealthObserverRemote);
  });

  test('ObserveBatteryChargeStatus', () => {
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    const batteryChargeStatusObserverRemote =
        /** @type {!BatteryChargeStatusObserver} */ ({
          onBatteryChargeStatusUpdated: (batteryChargeStatus) => {
            assertDeepEquals(fakeBatteryChargeStatus[0], batteryChargeStatus);
          }
        });

    return provider.observeBatteryChargeStatus(
        batteryChargeStatusObserverRemote);
  });

  test('ObserveCpuUsage', () => {
    provider.setFakeCpuUsage(fakeCpuUsage);

    const cpuObserverRemote = /** @type {!CpuUsageObserver} */ ({
      onCpuUsageUpdated: (cpuUsage) => {
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);
      }
    });

    return provider.observeCpuUsage(cpuObserverRemote);
  });

  test('ObserveMemoryUsage', () => {
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    const memoryUsageObserverRemote = /** @type {!MemoryUsageObserver} */ ({
      onMemoryUsageUpdated: (memoryUsage) => {
        assertDeepEquals(fakeMemoryUsage[0], memoryUsage);
      }
    });

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

    return provider.getBatteryInfo().then((result) => {
      assertDeepEquals(fakeBatteryInfo2, result.batteryInfo);
    });
  });

  test('CallMethodFirstThenDifferentDataAfterReset', () => {
    // Setup the initial fake data.
    provider.setFakeBatteryInfo(fakeBatteryInfo);
    return provider.getBatteryInfo()
        .then((result) => {
          assertDeepEquals(fakeBatteryInfo, result.batteryInfo);
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
          return provider.getBatteryInfo().then((result) => {
            assertDeepEquals(fakeBatteryInfo2, result.batteryInfo);
          });
        });
  });

  test('ObserveCpuTwiceWithTrigger', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeCpuUsage.length >= 2);
    provider.setFakeCpuUsage(fakeCpuUsage);

    // Keep track of which observation we should get.
    let whichSample = 0;
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    /** @type {!CpuUsageObserver} */
    const cpuObserverRemote = /** @type {!CpuUsageObserver} */ ({
      onCpuUsageUpdated: (cpuUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeCpuUsage[whichSample], cpuUsage);

        if (whichSample === 0) {
          firstResolver.resolve();
        } else {
          completeResolver.resolve();
        }
        whichSample++;
      }
    });

    return provider.observeCpuUsage(cpuObserverRemote)
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerCpuUsageObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveMemoryTwiceWithTrigger', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeMemoryUsage.length >= 2);
    provider.setFakeMemoryUsage(fakeMemoryUsage);

    // Keep track of which observation we should get.
    let whichSample = 0;
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    const memoryObserverRemote = /** @type {!MemoryUsageObserver} */ ({
      onMemoryUsageUpdated: (memoryUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeMemoryUsage[whichSample], memoryUsage);

        if (whichSample === 0) {
          firstResolver.resolve();
        } else {
          completeResolver.resolve();
        }
        whichSample++;
      }
    });

    return provider.observeMemoryUsage(memoryObserverRemote)
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerMemoryUsageObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveBatteryHealthTwiceWithTrigger', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeBatteryHealth.length >= 2);
    provider.setFakeBatteryHealth(fakeBatteryHealth);

    // Keep track of which observation we should get.
    let whichSample = 0;
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    const batteryHealthObserverRemote = /** @type {!BatteryHealthObserver} */ ({
      onBatteryHealthUpdated: (batteryHealth) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeBatteryHealth[whichSample], batteryHealth);

        if (whichSample === 0) {
          firstResolver.resolve();
        } else {
          completeResolver.resolve();
        }
        whichSample++;
      }
    });

    return provider.observeBatteryHealth(batteryHealthObserverRemote)
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerBatteryHealthObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveBatteryChargeStatusTwiceWithTrigger', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeBatteryChargeStatus.length >= 2);
    provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);

    // Keep track of which observation we should get.
    let whichSample = 0;
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    const batteryChargeStatusObserverRemote =
        /** @type {!BatteryChargeStatusObserver} */ ({
          onBatteryChargeStatusUpdated: (batteryChargeStatus) => {
            // Only expect 2 calls.
            assertTrue(whichSample >= 0);
            assertTrue(whichSample <= 1);
            assertDeepEquals(
                fakeBatteryChargeStatus[whichSample], batteryChargeStatus);

            if (whichSample === 0) {
              firstResolver.resolve();
            } else {
              completeResolver.resolve();
            }
            whichSample++;
          }
        });

    return provider
        .observeBatteryChargeStatus(batteryChargeStatusObserverRemote)
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerBatteryChargeStatusObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveWithResetBetweenTriggers', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeCpuUsage.length >= 2);
    provider.setFakeCpuUsage(fakeCpuUsage);

    // Keep track of which call to the callback.
    let whichSample = 0;
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    /** @type {!CpuUsageObserver} */
    const cpuObserverRemote = /** @type {!CpuUsageObserver} */ ({
      onCpuUsageUpdated: (cpuUsage) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);

        // In both cases we should get the first sample only.
        assertDeepEquals(fakeCpuUsage[0], cpuUsage);

        if (whichSample === 0) {
          firstResolver.resolve();
        } else {
          completeResolver.resolve();
        }
        whichSample++;
      }
    });

    return provider.observeCpuUsage(cpuObserverRemote)
        .then(() => {
          return firstResolver.promise;
        })
        .then(() => {
          // After the observer fires the first time, reset it and observe
          // again. We should go back to the initial sample.
          provider.reset();
          provider.setFakeCpuUsage(fakeCpuUsage);

          // Observing will implicitly trigger again.
          return provider.observeCpuUsage(cpuObserverRemote);
        })
        .then(() => {
          // Wait for the second callback which should return the first sample
          // because it was reset.
          return completeResolver.promise;
        });
  });
}
