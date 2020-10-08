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
 *   getBatteryInfo: !function(): !Promise<!BatteryInfo>,
 *   getSystemInfo: !function(): !Promise<!SystemInfo>,
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
 *   has_battery: boolean,
 * }}
 */
export let DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {{
 *   milestone_version: string,
 * }}
 */
export let VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {{
 *   board_name: string,
 *   cpu_model_name: string,
 *   cpu_threads_count: number,
 *   device_capabilities: DeviceCapabilities,
 *   total_memory_kib: number,
 *   version: VersionInfo,
 * }}
 */
export let SystemInfo;

/**
 * Type alias for BatteryInfo.
 * @typedef {{
 *   charge_full_design_milliamp_hours: number,
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
 *   cpu_temp_degrees_celcius: number,
 *   percent_usage_system: number,
 *   percent_usage_user: number,
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
 * Type alias for BatteryChargeStatus.
 * @typedef {{
 *   charge_full_now_milliamp_hours: number,
 *   charge_now_milliamp_hours: number,
 *   current_now_milliamps: number,
 *   power_adapter_status: ExternalPowerSource,
 *   power_time: string,
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
 *   battery_wear_percentage: number,
 *   charge_full_design_milliamp_hours: number,
 *   charge_full_now_milliamp_hours: number,
 *   cycle_count: number,
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
 *   available_memory_kib: number,
 *   free_memory_kib: number,
 *   total_memory_kib: number,
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
 *   simple_result: !StandardRoutineResult
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
