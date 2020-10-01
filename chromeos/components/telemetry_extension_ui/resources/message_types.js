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
  DIAGNOSTICS_RUN_FP_ACCURACY_ROUTINE:
      'DiagnosticsService.RunFloatingPointAccuraryRoutine',
  DIAGNOSTICS_RUN_NVME_WEAR_LEVEL_ROUTINE:
      'DiagnosticsService.RunNvmeWearLevelRoutine',
  DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE:
      'DiagnosticsService.RunNvmeSelfTestRoutine',
  DIAGNOSTICS_RUN_DISK_READ_ROUTINE: 'DiagnosticsService.RunDiskReadRoutine',
  DIAGNOSTICS_RUN_PRIME_SEARCH_ROUTINE:
      'DiagnosticsService.RunPrimeSearchRoutine',
  DIAGNOSTICS_RUN_BATTERY_DISCHARGE_ROUTINE:
      'DiagnosticsService.RunBatteryDischargeRoutine',
  DIAGNOSTICS_RUN_BATTERY_CHARGE_ROUTINE:
      'DiagnosticsService.RunBatteryChargeRoutine',
  PROBE_TELEMETRY_INFO: 'ProbeService.ProbeTelemetryInfo',
  SYSTEM_EVENTS_SERVICE_EVENTS: 'SystemEventsService.Events',
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
 * Request message sent by the unprivileged context to the privileged
 * context to run floating point accuracy routine.
 * @typedef {{ duration: !number }}
 */
dpsl_internal.DiagnosticsRunFPAccuracyRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run NVMe wear level routine.
 * @typedef {{ wearLevelThreshold: !number }}
 */
dpsl_internal.DiagnosticsRunNvmeWearLevelRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run NVMe self test routine.
 * @typedef {{ nvmeSelfTestType: !string }}
 */
dpsl_internal.DiagnosticsRunNvmeSelfTestRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run disk read routine.
 * @typedef {{
 * type: !string,
 * lengthSeconds: !number,
 * fileSizeMb: !number}}
 */
dpsl_internal.DiagnosticsRunDiskReadRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run prime search routine.
 * @typedef {{
 * lengthSeconds: !number,
 * maximumNumber: !number}}
 */
dpsl_internal.DiagnosticsRunPrimeSearchRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run battery discharge routine.
 * @typedef {{
 * lengthSeconds: !number,
 * maximumDischargePercentAllowed: !number}}
 */
dpsl_internal.DiagnosticsRunBatteryDischargeRoutineRequest;

/**
 * Request message sent by the unprivileged context to the privileged
 * context to run battery charge routine.
 * @typedef {{
 * lengthSeconds: !number,
 * minimumChargePercentRequired: !number}}
 */
dpsl_internal.DiagnosticsRunBatteryChargeRoutineRequest;

/**
 * Response message sent by the privileged context containing routine
 * information.
 * @typedef { !Object | !Error }
 */
dpsl_internal.DiagnosticsRunRoutineResponse;

/**
 * Event response from System Events Service.
 * @typedef {{ type: !string }}
 */
dpsl_internal.Event;

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
