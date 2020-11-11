// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(zentaro): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

/**
 * Type of SystemDataProviderInterface.ObserveBatteryChargeStatus function.
 * @typedef {!function(!BatteryChargeStatusObserver): !Promise}
 */
export let ObserveBatteryChargeStatusFunction;

/**
 * Type of SystemDataProviderInterface.ObserveBatteryHealth function.
 * @typedef {!function(!BatteryHealthObserver): !Promise}
 */
export let ObserveBatteryHealthFunction;

/**
 * Type of SystemDataProviderInterface.ObserveCpuUsage function.
 * @typedef {!function(!CpuUsageObserver): !Promise}
 */
export let ObserveCpuUsageFunction;

/**
 * Type of SystemDataProviderInterface.ObserveMemoryUsage function.
 * @typedef {!function(!MemoryUsageObserver): !Promise}
 */
export let ObserveMemoryUsageFunction;

/**
 * Type alias for the SystemDataProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 * @typedef {{
 *   getBatteryInfo: !function(): !Promise<!{batteryInfo: !BatteryInfo}>,
 *   getSystemInfo: !function(): !Promise<!{systemInfo: SystemInfo}>,
 *   observeBatteryChargeStatus: !ObserveBatteryChargeStatusFunction,
 *   observeBatteryHealth: !ObserveBatteryHealthFunction,
 *   observeCpuUsage: !ObserveCpuUsageFunction,
 *   observeMemoryUsage: !ObserveMemoryUsageFunction,
 * }}
 */
export let SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {{
 *   hasBattery: boolean,
 * }}
 */
export let DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {{
 *   milestoneVersion: string,
 * }}
 */
export let VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {{
 *   boardName: string,
 *   cpuModelName: string,
 *   cpuThreadsCount: number,
 *   deviceCapabilities: DeviceCapabilities,
 *   marketingName: string,
 *   totalMemoryKib: number,
 *   versionInfo: VersionInfo,
 * }}
 */
export let SystemInfo;

/**
 * Type alias for BatteryInfo.
 * @typedef {{
 *   chargeFullDesignMilliampHours: number,
 *   manufacturer: string,
 * }}
 */
export let BatteryInfo;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {{
 *   onCpuUsageUpdated: !function(!CpuUsage),
 * }}
 */
export let CpuUsageObserver;

/**
 * Type alias for CpuUsage.
 * @typedef {{
 *   cpuTempDegreesCelsius: number,
 *   percentUsageSystem: number,
 *   percentUsageUser: number,
 * }}
 */
export let CpuUsage;

/**
 * Type alias for BatteryChargeStatusObserver.
 * @typedef {{
 *   onBatteryChargeStatusUpdated: !function(!BatteryChargeStatus)
 * }}
 */
export let BatteryChargeStatusObserver;

/**
 * External power source enumeration.
 * @enum {number}
 */
export let ExternalPowerSource = {
  kAc: 0,
  kUsb: 1,
  kDisconnected: 2,
};

/**
 * Battery state enumeration.
 * @enum {number}
 */
export let BatteryState = {
  kCharging: 0,
  kDischarging: 1,
  kFull: 2,
};

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {{
 *   batteryState: BatteryState,
 *   chargeFullNowMilliampHours: number,
 *   chargeNowMilliampHours: number,
 *   currentNowMilliamps: number,
 *   powerAdapterStatus: ExternalPowerSource,
 *   powerTime: string,
 * }}
 */
export let BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {{
 *   onBatteryHealthUpdated: !function(!BatteryHealth)
 * }}
 */
export let BatteryHealthObserver;

/**
 * Type alias for BatteryHealth.
 * @typedef {{
 *   batteryWearPercentage: number,
 *   chargeFullDesignMilliampHours: number,
 *   chargeFullNowMilliampHours: number,
 *   cycleCount: number,
 * }}
 */
export let BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {{
 *   onMemoryUsageUpdated: !function(!MemoryUsage)
 * }}
 */
export let MemoryUsageObserver;

/**
 * Type alias for MemoryUsage.
 * @typedef {{
 *   availableMemoryKib: number,
 *   freeMemoryKib: number,
 *   totalMemoryKib: number,
 * }}
 */
export let MemoryUsage;

/**
 * Enumeration of routines.
 * @enum {number}
 */
export let RoutineName = {
  kCpuStress: 0,
  kCpuCache: 1,
  kFloatingPoint: 2,
  kPrimeSearch: 3,
  kMemory: 4,
  kPower: 5,
  kCharge: 6,
  kDischarge: 7,
};

/**
 * Type alias for StandardRoutineResult.
 * @enum {number}
 */
export let StandardRoutineResult = {
  kTestPassed: 0,
  kTestFailed: 1,
  kErrorExecuting: 2,
  kUnableToRun: 3,
};

/**
 * Type alias for RoutineResult.
 * TODO(zentaro): Currently only includes simple result type.
 * @typedef {{
 *   simpleResult: !StandardRoutineResult
 * }}
 */
export let RoutineResult;

/**
 * Type alias for RoutineResultInfo.
 * @typedef {{
 *   name: !RoutineName,
 *   result: !RoutineResult,
 * }}
 */
export let RoutineResultInfo;

/**
 * Type of RoutineRunner.onRoutineResult function.
 * @typedef {!function(!RoutineResultInfo)}
 */
export let RoutineResultFunction;

/**
 * Type alias for RoutineRunner.
 * @typedef {{
 *   onRoutineResult: !RoutineResultFunction,
 * }}
 */
export let RoutineRunner;

/**
 * Type of SystemRoutineController.RunRoutine function.
 * @typedef {!function(!RoutineName, !RoutineRunner): !Promise}
 */
export let RunRoutineFunction;

/**
 * Type alias for SystemRoutineControllerInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 * @typedef {{
 *   runRoutine: !RunRoutineFunction,
 * }}
 */
export let SystemRoutineControllerInterface;
