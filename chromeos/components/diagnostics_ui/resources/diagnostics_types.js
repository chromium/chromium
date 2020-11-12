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
 * Type alias for the SystemDataProviderInterface.
 * @typedef {chromeos.diagnostics.mojom.SystemDataProviderInterface}
 */
export let SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {chromeos.diagnostics.mojom.DeviceCapabilities}
 */
export let DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {chromeos.diagnostics.mojom.VersionInfo}
 */
export let VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {chromeos.diagnostics.mojom.SystemInfo}
 */
export let SystemInfo;

/**
 * Type alias for ExternalPowerSource.
 * @typedef {chromeos.diagnostics.mojom.ExternalPowerSource}
 */
export let ExternalPowerSource;

/**
 * Type alias for BatteryState.
 * @typedef {chromeos.diagnostics.mojom.BatteryState}
 */
export let BatteryState;

/**
 * Type alias for BatteryInfo.
 * @typedef {chromeos.diagnostics.mojom.BatteryInfo}
 */
export let BatteryInfo;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserver}
 */
export let BatteryChargeStatusObserver;

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatus}
 */
export let BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserver}
 */
export let BatteryHealthObserver;

/**
 * Type alias for BatteryHealth.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealth}
 */
export let BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserver}
 */
export let MemoryUsageObserver;

/**
 * Type alias for MemoryUsage.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsage}
 */
export let MemoryUsage;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserver}
 */
export let CpuUsageObserver;

/**
 * Type alias for CpuUsage.
 * @typedef {chromeos.diagnostics.mojom.CpuUsage}
 */
export let CpuUsage;

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
