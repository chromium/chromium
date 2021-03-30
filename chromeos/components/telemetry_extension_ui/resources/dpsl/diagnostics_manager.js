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
   * @type {!string}
   * @const
   * @private
   */
  const ROUTINE_STATUS_WAITING_USER_ACTION = 'waiting';

  /**
   * @param {!string} messageName
   * @param {(!Object|undefined)=} message
   * @returns {!Object}
   */
  async function genericSendMessage(messageName, message) {
    const response = await messagePipe.sendMessage(messageName, message);
    if (response instanceof Error) {
      throw response;
    }
    return /** @type {!Object} */ (response);
  }

  /**
   * Keeps track of Routine status when running dpsl.diagnostics.* diagnostics
   * routines.
   */
  class Routine {
    /**
     * @param {!number} id
     * @private
     */
    constructor(id) {
      /**
       * Routine ID created when the routine is first requested to run.
       * @type { !number }
       * @const
       * @private
       */
      this.id = id;
    }

    /**
     * Sends |command| on this routine to the backend.
     * @param {!string} command
     * @returns {!Promise<!dpsl.RoutineStatus>}
     * @private
     */
    async _genericSendCommand(command) {
      const message =
          /** @type {!dpsl_internal.DiagnosticsGetRoutineUpdateRequest} */ ({
          routineId: this.id,
          command: command,
          includeOutput: true
        });
      const response =
          /**
            @type {{
            progressPercent: number,
            output: string,
            routineUpdateUnion: ({interactiveUpdate: {userMessage:
            string}}|{noninteractiveUpdate:{status: string, statusMessage:
            string}})
            }}
          */
          (await genericSendMessage(
              dpsl_internal.Message.DIAGNOSTICS_ROUTINE_UPDATE, message));

      let status = /** @type {dpsl.RoutineStatus} */ ({
        progressPercent: 0,
        output: '',
        status: '',
        statusMessage: '',
        userMessage: ''
      });

      // fill in the status object and return it.
      status.progressPercent = response.progressPercent;
      status.output = response.output || '';
      if (response.routineUpdateUnion.noninteractiveUpdate) {
        status.status = response.routineUpdateUnion.noninteractiveUpdate.status;
        status.statusMessage =
            response.routineUpdateUnion.noninteractiveUpdate.statusMessage;
      } else {
        status.userMessage =
            response.routineUpdateUnion.interactiveUpdate.userMessage;
        status.status = ROUTINE_STATUS_WAITING_USER_ACTION;
      }
      return status;
    }

    /**
     * Returns current status of this routine.
     * @return { !Promise<!dpsl.RoutineStatus> }
     * @public
     */
    async getStatus() {
      return this._genericSendCommand('get-status');
    }

    /**
     * Resumes this routine, e.g. when user prompts to run a waiting routine.
     * @return { !Promise<!dpsl.RoutineStatus> }
     * @public
     */
    async resume() {
      return this._genericSendCommand('continue');
    }

    /**
     * Stops this routine, if running, or remove otherwise.
     * Note: The routine cannot be restarted again.
     * @return { !Promise<!dpsl.RoutineStatus> }
     * @public
     */
    async stop() {
      this._genericSendCommand('cancel');
      return this._genericSendCommand('remove');
    }
  }

  /**
   * @param {!string} messageName
   * @param {(!Object|undefined)=} message
   * @returns {!Promise<!Routine>}
   */
  async function genericRunRoutine(messageName, message) {
    const response =
        /** @type {{id: number, status: string}} */ (
            await genericSendMessage(messageName, message));
    return new Routine(response.id);
  }

  /**
   * Diagnostics Battery Manager for dpsl.diagnostics.battery.* APIs.
   */
  class BatteryManager {
    /**
     * Runs battery capacity test.
     * @return { !Promise<!Routine> }
     * @public
     */
    async runCapacityRoutine() {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CAPACITY_ROUTINE);
    }

    /**
     * Runs battery health test.
     * @return { !Promise<!Routine> }
     * @public
     */
    async runHealthRoutine() {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_HEALTH_ROUTINE);
    }

    /**
     * Runs battery capacity test.
     * @param {!dpsl.BatteryDischargeRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runDischargeRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_DISCHARGE_ROUTINE,
          params);
    }

    /**
     * Runs battery charge test.
     * @param {!dpsl.BatteryChargeRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runChargeRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CHARGE_ROUTINE, params);
    }
  }

  /**
   * Diagnostics NVME Manager for dpsl.diagnostics.nmve.* APIs.
   */
  class NvmeManager {
    /**
     * Runs NVMe smartctl test.
     * @return { !Promise<!Routine> }
     * @public
     */
    async runSmartctlCheckRoutine() {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_SMARTCTL_CHECK_ROUTINE);
    }

    /**
     * Runs NVMe wear level test.
     * @param {!dpsl.NvmeWearLevelRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runWearLevelRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_WEAR_LEVEL_ROUTINE,
          params);
    }

    /**
     * Runs NVMe short-self-test type test.
     * @return { !Promise<!Routine> }
     * @public
     */
    async runShortSelfTestRoutine() {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE,
          {nvmeSelfTestType: 'short-self-test'});
    }

    /**
     * Runs NVMe long-self-test type test.
     * @return { !Promise<!Routine> }
     * @public
     */
    async runLongSelfTestRoutine() {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE,
          {nvmeSelfTestType: 'long-self-test'});
    }
  }

  /**
   * Diagnostics Power Manager for dpsl.diagnostics.power.* APIs.
   */
  class PowerManager {
    /**
     * @param {(!dpsl.PowerAcRoutineParams)=} params
     * @returns {?string}
     * @private
     */
    _getExpectedPowerType(params) {
      if (!params || !params.expectedPowerType)
        return null;
      return params.expectedPowerType;
    }

    /**
     * Runs power ac connected-type test.
     * @param {(!dpsl.PowerAcRoutineParams)=} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runAcConnectedRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_AC_POWER_ROUTINE, {
            expectedStatus: 'connected',
            expectedPowerType: this._getExpectedPowerType(params)
          });
    }

    /**
     * Runs power ac disconnected-type test.
     * @param {(!dpsl.PowerAcRoutineParams)=} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runAcDisconnectedRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_AC_POWER_ROUTINE, {
            expectedStatus: 'disconnected',
            expectedPowerType: this._getExpectedPowerType(params)
          });
    }
  }

  /**
   * Diagnostics CPU Manager for dpsl.diagnostics.cpu.* APIs.
   */
  class CpuManager {
    /**
     * Runs CPU cache test.
     * @param {!dpsl.CpuRoutineDurationParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runCacheRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_CACHE_ROUTINE, params);
    }

    /**
     * Runs CPU stress test.
     * @param {!dpsl.CpuRoutineDurationParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runStressRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_STRESS_ROUTINE, params);
    }

    /**
     * Runs CPU floating point accuracy test.
     * @param {!dpsl.CpuRoutineDurationParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runFloatingPointAccuracyRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_FP_ACCURACY_ROUTINE, params);
    }

    /**
     * Runs CPU prime number search test.
     * @param {!dpsl.CpuPrimeSearchRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runPrimeSearchRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_PRIME_SEARCH_ROUTINE, params);
    }
  }


  /**
   * Diagnostics Disk Manager for dpsl.diagnostics.disk.* APIs.
   */
  class DiskManager {
    /**
     * Runs disk linear read test.
     * @param {!dpsl.DiskReadRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runLinearReadRoutine(params) {
      // TODO: rename fileSizeMb -> fileSizeMB in trusted.js.
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_DISK_READ_ROUTINE, {
            type: 'linear-read',
            lengthSeconds: params.lengthSeconds,
            fileSizeMb: params.fileSizeMB
          });
    }

    /**
     * Runs disk random read test.
     * @param {!dpsl.DiskReadRoutineParams} params
     * @return { !Promise<!Routine> }
     * @public
     */
    async runRandomReadRoutine(params) {
      return genericRunRoutine(
          dpsl_internal.Message.DIAGNOSTICS_RUN_DISK_READ_ROUTINE, {
            type: 'random-read',
            lengthSeconds: params.lengthSeconds,
            fileSizeMb: params.fileSizeMB
          });
    }
  }

  /**
   * DPSL Diagnostics Manager for dpsl.diagnostics.* APIs.
   */
  class DPSLDiagnosticsManager {
    constructor() {
      /**
       * @type {!BatteryManager}
       * @public
       */
      this.battery = new BatteryManager();

      /**
       * @type {!NvmeManager}
       * @public
       */
      this.nvme = new NvmeManager();

      /**
       * @type {!PowerManager}
       * @public
       */
      this.power = new PowerManager();

      /**
       * @type {!CpuManager}
       * @public
       */
      this.cpu = new CpuManager();

      /**
       * @type {!DiskManager}
       * @public
       */
      this.disk = new DiskManager();
    }

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
      console.warn(
          'chromeos.diagnostics.runBatteryCapacityRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.battery.runCapacityRoutine, instead');
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CAPACITY_ROUTINE));
    }

    /**
     * Requests battery health routine to be run.
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryHealthRoutine() {
      console.warn(
          'chromeos.diagnostics.runBatteryHealthRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.battery.runHealthRoutine, instead');

      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_HEALTH_ROUTINE));
    }

    /**
     * Requests smartctl routine to be run.
     * @return { !Promise<!Object> }
     * @public
     */
    async runSmartctlCheckRoutine() {
      console.warn(
          'chromeos.diagnostics.runSmartctlCheckRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.nvme.runSmartctlCheckRoutine, instead');

      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_SMARTCTL_CHECK_ROUTINE));
    }

    /**
     * Requests ac power routine to be run.
     * @param { !string } expectedStatus
     * @param { !string= } expectedPowerType
     * @return { !Promise<!Object> }
     * @public
     */
    async runAcPowerRoutine(expectedStatus, expectedPowerType) {
      console.warn(
          'chromeos.diagnostics.runAcPowerRoutine API function is deprecated',
          'and will be removed. Use dpsl.diagnostics.power.runAc*, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunAcPowerRoutineRequest}
               */
          ({
            expectedStatus: expectedStatus,
            expectedPowerType: expectedPowerType
          });
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_AC_POWER_ROUTINE, message));
    }

    /**
     * Requests cpu cache routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runCpuCacheRoutine(duration) {
      console.warn(
          'chromeos.diagnostics.runCpuCacheRoutine API function is deprecated',
          'and will be removed. Use dpsl.diagnostics.cpu.runCacheRoutine,',
          'instead');

      const message =
          /** @type {!dpsl_internal.DiagnosticsRunCpuCacheRoutineRequest} */ (
              {duration: duration});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_CACHE_ROUTINE, message));
    }

    /**
     * Requests cpu stress routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runCpuStressRoutine(duration) {
      console.warn(
          'chromeos.diagnostics.runCpuStressRoutine API function is deprecated',
          'and will be removed. Use dpsl.diagnostics.cpu.runStressRoutine,',
          'instead');

      const message =
          /** @type {!dpsl_internal.DiagnosticsRunCpuStressRoutineRequest} */ (
              {duration: duration});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_STRESS_ROUTINE, message));
    }

    /**
     * Requests floating point accuracy routine to be run for duration seconds.
     * @param { !number } duration
     * @return { !Promise<!Object> }
     * @public
     */
    async runFloatingPointAccuracyRoutine(duration) {
      console.warn(
          'chromeos.diagnostics.runFloatingPointAccuracyRoutine API function',
          'is deprecated and will be removed. Use',
          'dpsl.diagnostics.cpu.runFloatingPointAccuracyRoutine, instead');

      const message =
          /** @type {!dpsl_internal.DiagnosticsRunFPAccuracyRoutineRequest} */
          ({duration: duration});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_FP_ACCURACY_ROUTINE, message));
    }

    /**
     * Requests NVMe wear level routine to be run.
     * @param { !number } wearLevelThreshold
     * @return { !Promise<!Object> }
     * @public
     */
    async runNvmeWearLevelRoutine(wearLevelThreshold) {
      console.warn(
          'chromeos.diagnostics.runNvmeWearLevelRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.nvme.runWearLevelRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunNvmeWearLevelRoutineRequest}
           */
          ({wearLevelThreshold: wearLevelThreshold});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_WEAR_LEVEL_ROUTINE,
          message));
    }

    /**
     * Requests NVMe self test routine to be run.
     * @param { !string } nvmeSelfTestType
     * @return { !Promise<!Object> }
     * @public
     */
    async runNvmeSelfTestRoutine(nvmeSelfTestType) {
      console.warn(
          'chromeos.diagnostics.runNvmeSelfTestRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.nvme.run{Short/Long}SelfTestRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunNvmeSelfTestRoutineRequest}
           */
          ({nvmeSelfTestType: nvmeSelfTestType});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE,
          message));
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
      console.warn(
          'chromeos.diagnostics.runDiskReadRoutine API function is deprecated',
          'and will be removed. Use',
          'dpsl.diagnostics.disk.run{Linear/Random}ReadRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunDiskReadRoutineRequest}
           */
          ({type: type, lengthSeconds: lengthSeconds, fileSizeMb: fileSizeMb});

      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_DISK_READ_ROUTINE, message));
    }

    /**
     * Requests prime search routine to be run.
     * @param { !number } lengthSeconds
     * @param { !number } maximumNumber
     * @return { !Promise<!Object> }
     * @public
     */
    async runPrimeSearchRoutine(lengthSeconds, maximumNumber) {
      console.warn(
          'chromeos.diagnostics.runPrimeSearchRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.cpu.runPrimeSearchRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunPrimeSearchRoutineRequest}
           */
          ({lengthSeconds: lengthSeconds, maximumNumber: maximumNumber});
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_PRIME_SEARCH_ROUTINE, message));
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
      console.warn(
          'chromeos.diagnostics.runBatteryDischargeRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.battery.runDischargeRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunBatteryDischargeRoutineRequest}
           */
          ({
            lengthSeconds: lengthSeconds,
            maximumDischargePercentAllowed: maximumDischargePercentAllowed
          });
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_DISCHARGE_ROUTINE,
          message));
    }

    /**
     * Requests battery charge routine to be run.
     * @param { !number } lengthSeconds
     * @param { !number } minimumChargePercentRequired
     * @return { !Promise<!Object> }
     * @public
     */
    async runBatteryChargeRoutine(lengthSeconds, minimumChargePercentRequired) {
      console.warn(
          'chromeos.diagnostics.runBatteryChargeRoutine API function is',
          'deprecated and will be removed. Use',
          'dpsl.diagnostics.battery.runChargeRoutine, instead');

      const message =
          /**
             @type {!dpsl_internal.DiagnosticsRunBatteryChargeRoutineRequest}
           */
          ({
            lengthSeconds: lengthSeconds,
            minimumChargePercentRequired: minimumChargePercentRequired
          });
      return /** @type {!Object} */ (await genericSendMessage(
          dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CHARGE_ROUTINE,
          message));
    }
  };

  globalThis.chromeos.diagnostics = new DiagnosticsManager();
})();
