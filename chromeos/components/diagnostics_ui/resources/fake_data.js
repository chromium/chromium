// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatus, BatteryHealth, CpuUsage, ExternalPowerSource, MemoryUsage, RoutineName, StandardRoutineResult, SystemInfo} from './diagnostics_types.js'
import {stringToMojoString16} from './mojo_utils.js';

/* @type {!Array<!BatteryChargeStatus>} */
export const fakeBatteryChargeStatus = [
  {
    chargeFullNowMilliampHours: 5700,
    chargeNowMilliampHours: 4200,
    currentNowMilliamps: 1123,
    powerAdapterStatus: chromeos.diagnostics.mojom.ExternalPowerSource.kAc,
    powerTime: stringToMojoString16('3h 15m'),
  },
  {
    chargeFullNowMilliampHours: 5700,
    chargeNowMilliampHours: 4500,
    currentNowMilliamps: 1123,
    powerAdapterStatus: chromeos.diagnostics.mojom.ExternalPowerSource.kAc,
    powerTime: stringToMojoString16('3h 01m'),
  },
  {
    chargeFullNowMilliampHours: 5700,
    chargeNowMilliampHours: 4800,
    currentNowMilliamps: 1123,
    powerAdapterStatus: chromeos.diagnostics.mojom.ExternalPowerSource.kAc,
    powerTime: stringToMojoString16('2h 45m'),
  }
];

/* @type {!Array<!BatteryHealth>} */
export const fakeBatteryHealth = [
  {
    batteryWearPercentage: 7,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5700,
    cycleCount: 73,
  },
  {
    battery_weabatteryWearPercentager_percentage: 8,
    chargeFullDesignMilliampHours: 6000,
    chargeFullNowMilliampHours: 5699,
    cycleCount: 73,
  }
];

/* @type {!BatteryInfo} */
export const fakeBatteryInfo = {
  chargeFullDesignMilliampHours: 6000,
  manufacturer: 'BatterCorp USA',
};

/* @type {!BatteryInfo} */
export const fakeBatteryInfo2 = {
  chargeFullDesignMilliampHours: 9000,
  manufacturer: 'PowerPod 9000',
};

/* @type {!Array<!CpuUsage>} */
export const fakeCpuUsage = [
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 15,
    percentUsageUser: 20,
  },
  {
    averageCpuTempCelsius: 106,
    percentUsageSystem: 30,
    percentUsageUser: 40,
  },
  {
    averageCpuTempCelsius: 107,
    percentUsageSystem: 31,
    percentUsageUser: 45,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 55,
    percentUsageUser: 24,
  },
  {
    averageCpuTempCelsius: 109,
    percentUsageSystem: 49,
    percentUsageUser: 10,
  },
  {
    averageCpuTempCelsius: 161,
    percentUsageSystem: 1,
    percentUsageUser: 99,
  },
  {
    averageCpuTempCelsius: 118,
    percentUsageSystem: 35,
    percentUsageUser: 37,
  },
  {
    averageCpuTempCelsius: 110,
    percentUsageSystem: 26,
    percentUsageUser: 30,
  },
];

/* @type {!Array<!MemoryUsage>} */
export const fakeMemoryUsage = [
  {
    availableMemoryKib: 57000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 52000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 53000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  },
  {
    availableMemoryKib: 65000,
    freeMemoryKib: 15000,
    totalMemoryKib: 128000,
  }
];

/* @type {!SystemInfo} */
export const fakeSystemInfo = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  deviceCapabilities: {hasBattery: true},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99'},
};

/* @type {!SystemInfo} */
export const fakeSystemInfoWithoutBattery = {
  boardName: 'CrOS Board',
  cpuModelName: 'BestCpu SoFast 1000',
  cpuThreadsCount: 8,
  deviceCapabilities: {hasBattery: false},
  marketingName: 'Coolest Chromebook',
  totalMemoryKib: 128000,
  versionInfo: {milestoneVersion: 'M99'},
};

/* @type {!Map<RoutineName, StandardRoutineResult} */
export const fakeRoutineResults = new Map([
  [RoutineName.kCpuStress, StandardRoutineResult.kTestPassed],
  [RoutineName.kCpuCache, StandardRoutineResult.kTestPassed],
  [RoutineName.kFloatingPoint, StandardRoutineResult.kTestFailed],
  [RoutineName.kPrimeSearch, StandardRoutineResult.kErrorExecuting],
  [RoutineName.kMemory, StandardRoutineResult.kTestPassed],
]);