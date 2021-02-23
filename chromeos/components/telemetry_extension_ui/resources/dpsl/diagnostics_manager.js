// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Diagnostics interface exposed to third-parties for running device diagnostics
 * routines (tests).
 */

(() => {
  const messagePipe = dpsl.internal.messagePipe;

  /**
   * DPSL Diagnostics Manager for dpsl.diagnostics.* APIs.
   */
  class DPSLDiagnosticsManager {
    constructor() {}

    /**
     * Requests a list of available diagnostics routines.
     * @return { !Promise<!dpsl.AvailableRoutinesList> }
     * @public
     */
    async getAvailableRoutines() {
      const response =
          /** @type {!dpsl.AvailableRoutinesList} */
          (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_AVAILABLE_ROUTINES));
      return response;
    }
  }

  globalThis.dpsl.diagnostics = new DPSLDiagnosticsManager();

  /**
   * DPSL Diagnostics Manager.
   */
  class DiagnosticsManager {
    constructor() {
      /**
       * @type {!DPSLDiagnosticsManager}
       */
      this.dpslDiagnosticsManager = /** @type {!DPSLDiagnosticsManager} */
        (globalThis.dpsl.diagnostics);
    }

    /**
     * Requests a list of available routines.
     * @return { !Promise<!dpsl.AvailableRoutinesList> }
     * @public
     */
    async getAvailableRoutines() {
      console.warn(
        'chromeos.diagnostics.getAvailableRoutines API function is deprecated',
        'and will be removed. Use dpsl.diagnostics.getAvailableRoutines,',
        'instead');
      return this.dpslDiagnosticsManager.getAvailableRoutines();
    }

    /**
     * Requests a command to be run on a diagnostic routine.
     * @param { !number } routineId
     * @param { !string } command
     * @param { !boolean } includeOutput
     * @return { !Promise<!Object> }
     * @public
     */
    async sendCommandToRoutine(routineId, command, includeOutput) {
      const message =
          /** @type {!dpsl_internal.DiagnosticsGetRoutineUpdateRequest} */ ({
            routineId: routineId,
            command: command,
            includeOutput: includeOutput,
          });
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_ROUTINE_UPDATE, message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests battery capacity routine to be run.
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryCapacityRoutine() {
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CAPACITY_ROUTINE));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests battery health routine to be run.
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryHealthRoutine() {
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_HEALTH_ROUTINE));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests smartctl routine to be run.
     * @return { !Promise<!Object> }
     * @public
     */
    async runSmartctlCheckRoutine() {
      const response =
          /** @type {!Object} */
          (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_SMARTCTL_CHECK_ROUTINE));
      return response;
    }

    /**
     * Requests ac power routine to be run.
     * @param { !string } expectedStatus
     * @param { !string= } expectedPowerType
     * @return { !Promise<!Object> }
     * @public
     */
    async runAcPowerRoutine(expectedStatus, expectedPowerType) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunAcPowerRoutineRequest}
               */
          ({
            expectedStatus: expectedStatus,
            expectedPowerType: expectedPowerType
          });
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_AC_POWER_ROUTINE, message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests cpu cache routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runCpuCacheRoutine(duration) {
      const message =
          /** @type {!dpsl_internal.DiagnosticsRunCpuCacheRoutineRequest} */ (
              {duration: duration});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_CACHE_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests cpu stress routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runCpuStressRoutine(duration) {
      const message =
          /** @type {!dpsl_internal.DiagnosticsRunCpuStressRoutineRequest} */ (
              {duration: duration});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_STRESS_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests floating point accuracy routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runFloatingPointAccuracyRoutine(duration) {
      const message =
          /** @type {!dpsl_internal.DiagnosticsRunFPAccuracyRoutineRequest} */
          ({duration: duration});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_FP_ACCURACY_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests NVMe wear level routine to be run.
     * @param { !number } wearLevelThreshold
     * @return { !Promise<!Object> }
     * @public
     */
    async runNvmeWearLevelRoutine(wearLevelThreshold) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunNvmeWearLevelRoutineRequest}
           */
          ({wearLevelThreshold: wearLevelThreshold});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_WEAR_LEVEL_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests NVMe self test routine to be run.
     * @param { !string } nvmeSelfTestType
     * @return { !Promise<!Object> }
     * @public
     */
    async runNvmeSelfTestRoutine(nvmeSelfTestType) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunNvmeSelfTestRoutineRequest}
           */
          ({nvmeSelfTestType: nvmeSelfTestType});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests disk read routine to be run.
     * @param { !string } type
     * @param { !number } lengthSeconds
     * @param { !number } fileSizeMb
     * @return { !Promise<!Object> }
     * @public
     */
    async runDiskReadRoutine(type, lengthSeconds, fileSizeMb) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunDiskReadRoutineRequest}
           */
          ({type: type, lengthSeconds: lengthSeconds, fileSizeMb: fileSizeMb});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_DISK_READ_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests prime search routine to be run.
     * @param { !number } lengthSeconds
     * @param { !number } maximumNumber
     * @return { !Promise<!Object> }
     * @public
     */
    async runPrimeSearchRoutine(lengthSeconds, maximumNumber) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunPrimeSearchRoutineRequest}
           */
          ({lengthSeconds: lengthSeconds, maximumNumber: maximumNumber});
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_PRIME_SEARCH_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests battery discharge routine to be run.
     * @param { !number } lengthSeconds
     * @param { !number } maximumDischargePercentAllowed
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryDischargeRoutine(
        lengthSeconds, maximumDischargePercentAllowed) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunBatteryDischargeRoutineRequest}
           */
          ({
            lengthSeconds: lengthSeconds,
            maximumDischargePercentAllowed: maximumDischargePercentAllowed
          });
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_DISCHARGE_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }

    /**
     * Requests battery charge routine to be run.
     * @param { !number } lengthSeconds
     * @param { !number } minimumChargePercentRequired
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryChargeRoutine(lengthSeconds, minimumChargePercentRequired) {
      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunBatteryChargeRoutineRequest}
           */
          ({
            lengthSeconds: lengthSeconds,
            minimumChargePercentRequired: minimumChargePercentRequired
          });
      const response =
          /** @type {!Object} */ (await messagePipe.sendMessage(
              dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CHARGE_ROUTINE,
              message));
      if (response instanceof Error) {
        throw response;
      }
      return response;
    }
  };

  globalThis.chromeos.diagnostics = new DiagnosticsManager();
})();
