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
