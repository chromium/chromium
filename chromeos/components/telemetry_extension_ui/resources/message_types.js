// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the TelemetryExtension
 * privileged/unprivileged pipe.
 */

/**
 * @const
 */
const dpsl_internal = {};

/**
 * Enum for message types.
 * @enum { string }
 */
dpsl_internal.Message = {
  DIAGNOSTICS_AVAILABLE_ROUTINES: 'DiagnosticsService.GetAvailableRoutines',
  DIAGNOSTICS_ROUTINE_UPDATE: 'DiagnosticsService.GetRoutineUpdate',
  DIAGNOSTICS_RUN_BATTERY_CAPACITY_ROUTINE:
      'DiagnosticsService.RunBatteryCapacityRoutine',
  DIAGNOSTICS_RUN_BATTERY_HEALTH_ROUTINE:
      'DiagnosticsService.RunBatteryHealthRoutine',
  DIAGNOSTICS_RUN_SMARTCTL_CHECK_ROUTINE:
      'DiagnosticsService.RunSmartctlCheckRoutine',
  DIAGNOSTICS_RUN_AC_POWER_ROUTINE: 'DiagnosticsService.RunAcPowerRoutine',
  DIAGNOSTICS_RUN_CPU_CACHE_ROUTINE: 'DiagnosticsService.RunCpuCacheRoutine',
  DIAGNOSTICS_RUN_CPU_STRESS_ROUTINE: 'DiagnosticsService.RunCpuStressRoutine',
  PROBE_TELEMETRY_INFO: 'ProbeService.ProbeTelemetryInfo',
};

/**
 * Request message sent by the unprivileged context to request the privileged
 * context to diagnostics to get available routines.
 * @typedef { null }
 */
dpsl_internal.DiagnosticsGetAvailableRoutinesRequest;

/**
 * Response message sent by the privileged context containing diagnostic
 * routine enums.
 * @typedef { !Array<!string> }
 */
dpsl_internal.DiagnosticsGetAvailableRoutinesResponse;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to request a routine update.
 * @typedef {{
 *   routineId: !number,
 *   command: !string,
 *   includeOutput: !boolean}}
 */
dpsl_internal.DiagnosticsGetRoutineUpdateRequest;

/**
 * Response message sent by the privileged context containing diagnostic
 * routine update information.
 * @typedef { !Object | !Error }
 */
dpsl_internal.DiagnosticsGetRoutineUpdateResponse;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run battery capacity routine.
 * @typedef {{
 *  lowMah: !number,
 *  highMah: !number}}
 */
dpsl_internal.DiagnosticsRunBatteryCapacityRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run battery health routine.
 * @typedef {{
 *  maximumCycleCount: !number,
 *  percentBatteryWearAllowed: !number}}
 */
dpsl_internal.DiagnosticsRunBatteryHealthRoutineRequest;

/**
 * Request message sent by the unprivileged context to request the privileged
 * context to diagnostics to run smartctl check routine.
 * @typedef { null }
 */
dpsl_internal.DiagnosticsRunSmartctlCheckRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run ac power routine.
 * @typedef {{
 *  expectedStatus: !string,
 *  expectedPowerType: string}}
 */
dpsl_internal.DiagnosticsRunAcPowerRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run cpu cache routine.
 * @typedef {{ duration: !number }}
 */
dpsl_internal.DiagnosticsRunCpuCacheRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run cpu stress routine.
 * @typedef {{ duration: !number }}
 */
dpsl_internal.DiagnosticsRunCpuStressRoutineRequest;

/**
 * Response message sent by the privileged context containing routine
 * information.
 * @typedef { !Object | !Error }
 */
dpsl_internal.DiagnosticsRunRoutineResponse;

/**
 * Request message sent by the unprivileged context to request the privileged
 * context to probe telemetry information
 * @typedef { !Array<!string> }
 */
dpsl_internal.ProbeTelemetryInfoRequest;

/**
 * Response message sent by the privileged context sending telemetry
 * information.
 * @typedef { !Object|!Error }
 */
dpsl_internal.ProbeTelemetryInfoResponse;
