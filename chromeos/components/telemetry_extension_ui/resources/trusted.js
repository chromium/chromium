// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let diagnosticsService = null;
let probeService = null;
let systemEventsService = null;

/**
 * Lazy creates pointer to remote implementation of diagnostics service.
 * @return {!chromeos.health.mojom.DiagnosticsServiceRemote}
 */
function getOrCreateDiagnosticsService() {
  if (diagnosticsService === null) {
    diagnosticsService = chromeos.health.mojom.DiagnosticsService.getRemote();
  }
  return /** @type {!chromeos.health.mojom.DiagnosticsServiceRemote} */ (
      diagnosticsService);
}

/**
 * Lazy creates pointer to remote implementation of probe service.
 * @return {!chromeos.health.mojom.ProbeServiceRemote}
 */
function getOrCreateProbeService() {
  if (probeService === null) {
    probeService = chromeos.health.mojom.ProbeService.getRemote();
  }
  return /** @type {!chromeos.health.mojom.ProbeServiceRemote} */ (
      probeService);
}

/**
 * Lazy creates pointer to remote implementation of system events service.
 * @return {!chromeos.health.mojom.SystemEventsServiceRemote}
 */
function getOrCreateSystemEventsService() {
  if (systemEventsService === null) {
    systemEventsService = chromeos.health.mojom.SystemEventsService.getRemote();
  }
  return /** @type {!chromeos.health.mojom.SystemEventsServiceRemote} */ (
      systemEventsService);
}

/**
 * Alias for Mojo RunRoutine response.
 * @typedef { !Promise<{response: !chromeos.health.mojom.RunRoutineResponse}>
 * }
 */
let RunRoutineResponsePromise;

/**
 * Proxying diagnostics requests between DiagnosticsRequester on
 * chrome-untrusted:// side with WebIDL types and DiagnosticsService on
 * chrome:// side with Mojo types.
 */
class DiagnosticsProxy {
  constructor() {
    const routineEnum = chromeos.health.mojom.DiagnosticRoutineEnum;

    /**
     * @type { !Map<!chromeos.health.mojom.DiagnosticRoutineEnum, !string> }
     * @const
     */
    this.enumToRoutineName_ = new Map([
      [routineEnum.kBatteryCapacity, 'battery-capacity'],
      [routineEnum.kBatteryHealth, 'battery-health'],
      [routineEnum.kSmartctlCheck, 'smartctl-check'],
      [routineEnum.kAcPower, 'ac-power'],
      [routineEnum.kCpuCache, 'cpu-cache'],
      [routineEnum.kCpuStress, 'cpu-stress'],
      [routineEnum.kFloatingPointAccuracy, 'floating-point-accuracy'],
      [routineEnum.kNvmeWearLevel, 'nvme-wear-level'],
      [routineEnum.kNvmeSelfTest, 'nvme-self-test'],
      [routineEnum.kDiskRead, 'disk-read'],
      [routineEnum.kPrimeSearch, 'prime-search'],
      [routineEnum.kBatteryDischarge, 'battery-discharge'],
      [routineEnum.kBatteryCharge, 'battery-charge'],
    ]);

    if (this.enumToRoutineName_.size !== routineEnum.MAX_VALUE + 1) {
      throw RangeError(
          'enumToRoutineName_ does not contain all items from enum!');
    }

    const commandEnum = chromeos.health.mojom.DiagnosticRoutineCommandEnum;

    /**
     * @type { !Map<!string,
     *     !chromeos.health.mojom.DiagnosticRoutineCommandEnum> }
     * @const
     */
    this.commandToEnum_ = new Map([
      ['continue', commandEnum.kContinue],
      ['cancel', commandEnum.kCancel],
      ['get-status', commandEnum.kGetStatus],
      ['remove', commandEnum.kRemove],
    ]);

    if (this.commandToEnum_.size !== commandEnum.MAX_VALUE + 1) {
      throw RangeError('commandToEnum_ does not contain all items from enum!');
    }

    const statusEnum = chromeos.health.mojom.DiagnosticRoutineStatusEnum;

    /**
     * @type { !Map<!chromeos.health.mojom.DiagnosticRoutineStatusEnum, !string>
     *     }
     * @const
     */
    this.enumToStatus_ = new Map([
      [statusEnum.kReady, 'ready'],
      [statusEnum.kRunning, 'running'],
      [statusEnum.kWaiting, 'waiting'],
      [statusEnum.kPassed, 'passed'],
      [statusEnum.kFailed, 'failed'],
      [statusEnum.kError, 'error'],
      [statusEnum.kCancelled, 'cancelled'],
      [statusEnum.kFailedToStart, 'failed-to-start'],
      [statusEnum.kRemoved, 'removed'],
      [statusEnum.kCancelling, 'cancelling'],
      [statusEnum.kUnsupported, 'unsupported'],
    ]);

    if (this.enumToStatus_.size !== statusEnum.MAX_VALUE + 1) {
      throw RangeError('enumToStatus_ does not contain all items from enum!');
    }

    const userMessageEnum =
        chromeos.health.mojom.DiagnosticRoutineUserMessageEnum;

    /**
     * @type { !Map<!chromeos.health.mojom.DiagnosticRoutineUserMessageEnum,
     *     !string> }
     * @const
     */
    this.enumToUserMessage_ = new Map([
      [userMessageEnum.kUnplugACPower, 'unplug-ac-power'],
      [userMessageEnum.kPlugInACPower, 'plug-in-ac-power'],
    ]);

    if (this.enumToUserMessage_.size !== userMessageEnum.MAX_VALUE + 1) {
      throw RangeError(
          'enumToUserMessage_ does not contain all items from enum!');
    }

    const acPowerStatusEnum = chromeos.health.mojom.AcPowerStatusEnum;

    /**
     * @type { !Map<!string, !chromeos.health.mojom.AcPowerStatusEnum> }
     * @const
     */
    this.acPowerStatusToEnum_ = new Map([
      ['connected', acPowerStatusEnum.kConnected],
      ['disconnected', acPowerStatusEnum.kDisconnected],
    ]);

    if (this.acPowerStatusToEnum_.size !== acPowerStatusEnum.MAX_VALUE + 1) {
      throw RangeError(
          'acPowerStatusToEnum_ does not contain all items from enum!');
    }

    const nvmeSelfTestTypeEnum = chromeos.health.mojom.NvmeSelfTestTypeEnum;

    /**
     * @type { !Map<!string, !chromeos.health.mojom.NvmeSelfTestTypeEnum> }
     * @const
     */
    this.nvmeSelfTestTypeToEnum_ = new Map([
      ['short-self-test', nvmeSelfTestTypeEnum.kShortSelfTest],
      ['long-self-test', nvmeSelfTestTypeEnum.kLongSelfTest],
    ]);

    if (this.nvmeSelfTestTypeToEnum_.size !==
        nvmeSelfTestTypeEnum.MAX_VALUE + 1) {
      throw RangeError(
          'nvmeSelfTestTypeToEnum_ does not contain all items from enum!');
    }

    const diskReadRoutineTypeEnum =
        chromeos.health.mojom.DiskReadRoutineTypeEnum;

    /**
     * @type { !Map<!string, !chromeos.health.mojom.DiskReadRoutineTypeEnum> }
     * @const
     */
    this.diskReadRoutineTypeToEnum_ = new Map([
      ['linear-read', diskReadRoutineTypeEnum.kLinearRead],
      ['random-read', diskReadRoutineTypeEnum.kRandomRead],
    ]);

    if (this.diskReadRoutineTypeToEnum_.size !==
        diskReadRoutineTypeEnum.MAX_VALUE + 1) {
      throw RangeError(
          'diskReadRoutineTypeToEnum_ does not contain all items from enum!');
    }
  }

  /**
   * @param { !Array<!chromeos.health.mojom.DiagnosticRoutineEnum> } routines
   * @return { !Array<!string> }
   */
  convertRoutines(routines) {
    return routines.map((routine) => {
      if (!this.enumToRoutineName_.has(routine)) {
        throw TypeError(`Diagnostic routine '${routine}' is unknown.`);
      }
      return this.enumToRoutineName_.get(routine);
    });
  }

  /**
   * Requests available routines.
   * @return { !Promise<dpsl_internal.DiagnosticsGetAvailableRoutinesResponse> }
   */
  async handleGetAvailableRoutines() {
    const availableRoutines =
        await getOrCreateDiagnosticsService().getAvailableRoutines();
    return this.convertRoutines(availableRoutines.availableRoutines);
  };

  /**
   * @param { !number } id
   * @return { !number }
   */
  convertRoutineId(id) {
    if (id < -2147483648 || id > 2147483647) {
      throw RangeError(`Diagnostic routine id '${id}' is out of int32 range.`);
    }
    return id;
  }

  /**
   * @param { !string } command
   * @return { !chromeos.health.mojom.DiagnosticRoutineCommandEnum }
   */
  convertCommandToEnum(command) {
    if (!this.commandToEnum_.has(command)) {
      throw TypeError(`Diagnostic command '${command}' is unknown.`);
    }

    return this.commandToEnum_.get(command);
  }

  /**
   * @param { !chromeos.health.mojom.DiagnosticRoutineStatusEnum } status
   * @return { !string | null }
   */
  convertStatus(status) {
    if (!this.enumToStatus_.has(status)) {
      return null;
    }

    return this.enumToStatus_.get(status);
  }

  /**
   * @param { !chromeos.health.mojom.DiagnosticRoutineUserMessageEnum }
   *     userMessage
   * @return { !string | null }
   */
  convertUserMessage(userMessage) {
    if (!this.enumToUserMessage_.has(userMessage)) {
      return null;
    }

    return this.enumToUserMessage_.get(userMessage);
  }

  /**
   * @param { !chromeos.health.mojom.RoutineUpdate } routineUpdate
   * @return { !Object }
   */
  convertRoutineUpdate(routineUpdate) {
    let result = {
      progressPercent: routineUpdate.progressPercent,
      output: routineUpdate.output,
      routineUpdateUnion: {}
    };

    const updateUnion = routineUpdate.routineUpdateUnion;

    if (typeof updateUnion.noninteractiveUpdate !== 'undefined' &&
        updateUnion.noninteractiveUpdate !== null) {
      let status = this.convertStatus(updateUnion.noninteractiveUpdate.status);

      result.routineUpdateUnion = {
        noninteractiveUpdate: {
          status: status,
          statusMessage: updateUnion.noninteractiveUpdate.statusMessage
        }
      };
    }

    if (typeof updateUnion.interactiveUpdate !== 'undefined' &&
        updateUnion.interactiveUpdate !== null) {
      let message =
          this.convertUserMessage(updateUnion.interactiveUpdate.userMessage);
      result.routineUpdateUnion = {interactiveUpdate: {userMessage: message}};
    }

    return result;
  }

  /**
   * Runs a command on a routine.
   * @param { !Object } message
   * @return { !Promise<dpsl_internal.DiagnosticsGetRoutineUpdateResponse> }
   */
  async handleGetRoutineUpdate(message) {
    const request =
        /** @type {dpsl_internal.DiagnosticsGetRoutineUpdateRequest} */ (
            message);

    let routine, command;
    try {
      routine = this.convertRoutineId(request.routineId);
      command = this.convertCommandToEnum(request.command);
    } catch (/** @type {!Error} */ error) {
      return error;
    }

    const response = await getOrCreateDiagnosticsService().getRoutineUpdate(
        routine, command, request.includeOutput);

    return this.convertRoutineUpdate(response.routineUpdate);
  };

  /**
   * @param { !chromeos.health.mojom.RunRoutineResponse } runRoutineResponse
   * @return { !Object }
   */
  convertRunRoutineResponse(runRoutineResponse) {
    return {
      id: runRoutineResponse.id,
      status: this.convertStatus(runRoutineResponse.status)
    };
  };

  /**
   * Generic handler for a runRoutine.
   * @param { !function(!Object): !RunRoutineResponsePromise } handler
   * @param { !Object } message
   * @return { !Promise<!dpsl_internal.DiagnosticsRunRoutineResponse> }
   */
  async genericRunRoutineHandler(handler, message) {
    try {
      const response = await handler(message);
      return this.convertRunRoutineResponse(response.response);
    } catch (/** @type !Error */ error) {
      return error;
    }
  };

  /**
   * Runs battery capacity routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunBatteryCapacityRoutine(message) {
    const request =
        /**
         * @type {!dpsl_internal.DiagnosticsRunBatteryCapacityRoutineRequest}
         */
        (message);
    return await getOrCreateDiagnosticsService().runBatteryCapacityRoutine(
        request.lowMah, request.highMah);
  };

  /**
   * Runs battery health routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunBatteryHealthRoutine(message) {
    const request =
        /**
         * @type {!dpsl_internal.DiagnosticsRunBatteryHealthRoutineRequest}
         */
        (message);
    return await getOrCreateDiagnosticsService().runBatteryHealthRoutine(
        request.maximumCycleCount, request.percentBatteryWearAllowed);
  };

  /**
   * Runs smartctl check routine.
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunSmartctlCheckRoutine() {
    return await getOrCreateDiagnosticsService().runSmartctlCheckRoutine();
  };

  /**
   * Converts expected status string to AcPowerStatusEnum.
   * @param { !string } expectedStatus
   * @return { !chromeos.health.mojom.AcPowerStatusEnum }
   */
  convertPowerStatusToEnum(expectedStatus) {
    if (!this.acPowerStatusToEnum_.has(expectedStatus)) {
      throw TypeError(
          `Diagnostic expected status '${expectedStatus}' is unknown.`);
    }

    return this.acPowerStatusToEnum_.get(expectedStatus);
  }

  /**
   * Runs ac power routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunAcPowerRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunAcPowerRoutineRequest} */ (
            message);
    const expectedStatus = this.convertPowerStatusToEnum(request.expectedStatus)
    return await getOrCreateDiagnosticsService().runAcPowerRoutine(
        expectedStatus, request.expectedPowerType);
  };

  /**
   * @param { !number } number
   */
  assertNumberIsPositive(number) {
    if (number <= 0) {
      throw RangeError(`Parameter must be positive.`);
    }
  }

  /**
   * Runs cpu cache routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunCpuCacheRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunCpuCacheRoutineRequest} */ (
            message);
    this.assertNumberIsPositive(request.duration);
    return await getOrCreateDiagnosticsService().runCpuCacheRoutine(
        request.duration);
  };

  /**
   * Runs cpu stress routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunCpuStressRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunCpuStressRoutineRequest} */ (
            message);
    this.assertNumberIsPositive(request.duration);
    return await getOrCreateDiagnosticsService().runCpuStressRoutine(
        request.duration);
  };

  /**
   * Runs floating point accuracy routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunFloatingPointAccuracyRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunFPAccuracyRoutineRequest} */
        (message);
    this.assertNumberIsPositive(request.duration);
    return await getOrCreateDiagnosticsService()
        .runFloatingPointAccuracyRoutine(request.duration);
  };

  /**
   * Runs NVMe wear level routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunNvmeWearLevelRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunNvmeWearLevelRoutineRequest} */
        (message);
    return await getOrCreateDiagnosticsService().runNvmeWearLevelRoutine(
        request.wearLevelThreshold);
  };

  /**
   * Converts NVMe self test type string to NvmeSelfTestTypeEnum.
   * @param { !string } nvmeSelfTestType
   * @return { !chromeos.health.mojom.NvmeSelfTestTypeEnum }
   */
  convertNvmeSelfTestTypeToEnum(nvmeSelfTestType) {
    if (!this.nvmeSelfTestTypeToEnum_.has(nvmeSelfTestType)) {
      throw TypeError(
          `Diagnostic NVMe self test type '${nvmeSelfTestType}' is unknown.`);
    }

    return this.nvmeSelfTestTypeToEnum_.get(nvmeSelfTestType);
  }

  /**
   * Runs NVMe self test routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunNvmeSelfTestRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunNvmeSelfTestRoutineRequest} */
        (message);
    return await getOrCreateDiagnosticsService().runNvmeSelfTestRoutine(
        this.convertNvmeSelfTestTypeToEnum(request.nvmeSelfTestType));
  };

  /**
   * Converts disk read type string to DiskReadRoutineTypeEnum.
   * @param { !string } type
   * @return { !chromeos.health.mojom.DiskReadRoutineTypeEnum }
   */
  convertDiskReadTypeToEnum(type) {
    if (!this.diskReadRoutineTypeToEnum_.has(type)) {
      throw TypeError(`Diagnostic disk read type '${type}' is unknown.`);
    }

    return this.diskReadRoutineTypeToEnum_.get(type);
  }

  /**
   * Throws an error if fileSizeMb exceeds maxSizeMb.
   * @param { !number } fileSizeMb
   * @param { !number } maxSizeMb
   */
  assertFileSizeLargerThan(fileSizeMb, maxSizeMb) {
    if (fileSizeMb > maxSizeMb) {
      throw RangeError(
          `Diagnostic disk read routine does not allow file sizes greater ` +
          `than '${maxSizeMb}'.`);
    }
  }

  /**
   * Runs disk read routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunDiskReadRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunDiskReadRoutineRequest} */
        (message);
    this.assertNumberIsPositive(request.lengthSeconds);
    this.assertFileSizeLargerThan(request.fileSizeMb, 10 * 1000);
    return await getOrCreateDiagnosticsService().runDiskReadRoutine(
        this.convertDiskReadTypeToEnum(request.type), request.lengthSeconds,
        request.fileSizeMb);
  };

  /**
   * Runs prime search routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunPrimeSearchRoutine(message) {
    const request =
        /** @type {!dpsl_internal.DiagnosticsRunPrimeSearchRoutineRequest} */
        (message);
    this.assertNumberIsPositive(request.lengthSeconds);
    return await getOrCreateDiagnosticsService().runPrimeSearchRoutine(
        request.lengthSeconds, request.maximumNumber);
  };

  /**
   * Runs battery discharge routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunBatteryDischargeRoutine(message) {
    const request =
        /**
           @type {!dpsl_internal.DiagnosticsRunBatteryDischargeRoutineRequest}
             */
        (message);
    this.assertNumberIsPositive(request.lengthSeconds);
    return await getOrCreateDiagnosticsService().runBatteryDischargeRoutine(
        request.lengthSeconds, request.maximumDischargePercentAllowed);
  };

  /**
   * Runs battery charge routine.
   * @param { !Object } message
   * @return { !RunRoutineResponsePromise }
   */
  async handleRunBatteryChargeRoutine(message) {
    const request =
        /**
           @type {!dpsl_internal.DiagnosticsRunBatteryChargeRoutineRequest}
             */
        (message);
    this.assertNumberIsPositive(request.lengthSeconds);
    return await getOrCreateDiagnosticsService().runBatteryChargeRoutine(
        request.lengthSeconds, request.minimumChargePercentRequired);
  };
};

const diagnosticsProxy = new DiagnosticsProxy();

/**
 * Proxying telemetry requests between TelemetryRequester on
 * chrome-untrusted:// side with WebIDL types and ProbeService on chrome://
 * side with Mojo types.
 */
class TelemetryProxy {
  constructor() {
    const categoryEnum = chromeos.health.mojom.ProbeCategoryEnum;

    /**
     * @type { !Map<!string, !chromeos.health.mojom.ProbeCategoryEnum> }
     * @const
     */
    this.categoryToEnum_ = new Map([
      ['battery', categoryEnum.kBattery],
      ['non-removable-block-devices', categoryEnum.kNonRemovableBlockDevices],
      ['cached-vpd-data', categoryEnum.kCachedVpdData],
      ['cpu', categoryEnum.kCpu], ['timezone', categoryEnum.kTimezone],
      ['memory', categoryEnum.kMemory], ['backlight', categoryEnum.kBacklight],
      ['fan', categoryEnum.kFan],
      ['stateful-partition', categoryEnum.kStatefulPartition],
      ['bluetooth', categoryEnum.kBluetooth]
    ]);

    if (this.categoryToEnum_.size != categoryEnum.MAX_VALUE + 1) {
      throw RangeError('categoryToEnum_ does not contain all items from enum!');
    }

    const errorEnum = chromeos.health.mojom.ErrorType;

    /**
     * @type { !Map<!chromeos.health.mojom.ErrorType, !string > }
     * @const
     */
    this.errorTypeToString_ = new Map([
      [errorEnum.kFileReadError, 'file-read-error'],
      [errorEnum.kParseError, 'parse-error'],
      [errorEnum.kSystemUtilityError, 'system-utility-error'],
      [errorEnum.kServiceUnavailable, 'service-unavailable'],
    ]);

    if (this.errorTypeToString_.size != errorEnum.MAX_VALUE + 1) {
      throw RangeError(
          'errorTypeToString_ does not contain all items from enum!');
    }

    const cpuArchEnum = chromeos.health.mojom.CpuArchitectureEnum;

    /**
     * @type { !Map<!chromeos.health.mojom.CpuArchitectureEnum, !string > }
     * @const
     */
    this.cpuArchToString_ = new Map([
      [cpuArchEnum.kUnknown, 'unknown'],
      [cpuArchEnum.kX86_64, 'x86-64'],
      [cpuArchEnum.kAArch64, 'AArch64'],
      [cpuArchEnum.kArmv7l, 'Armv7l'],
    ]);

    if (this.cpuArchToString_.size != cpuArchEnum.MAX_VALUE + 1) {
      throw RangeError(
          'cpuArchToString_ does not contain all items from enum!');
    }
  }

  /**
   * @param { !Array<!string> } categories
   * @return { !Array<!chromeos.health.mojom.ProbeCategoryEnum> }
   */
  convertCategories(categories) {
    return categories.map((category) => {
      if (!this.categoryToEnum_.has(category)) {
        throw TypeError(`Telemetry category '${category}' is unknown.`);
      }
      return this.categoryToEnum_.get(category);
    });
  }

  /**
   * @param { !chromeos.health.mojom.ErrorType } errorType
   * @return { !string }
   */
  convertErrorType(errorType) {
    if (!this.errorTypeToString_.has(errorType)) {
      throw TypeError(`Error type '${errorType}' is unknown.`);
    }
    return this.errorTypeToString_.get(errorType);
  }

  /**
   * @param { !chromeos.health.mojom.CpuArchitectureEnum } cpuArch
   * @return { !string }
   */
  convertCpuArch(cpuArch) {
    if (!this.cpuArchToString_.has(cpuArch)) {
      throw TypeError(`CPU architecture '${cpuArch}' is unknown.`);
    }
    return this.cpuArchToString_.get(cpuArch);
  }

  /**
   * @param { !chromeos.health.mojom.TelemetryInfo } telemetryInfo
   * @return { !Object }
   */
  convertAllEnums(telemetryInfo) {
    // Convert CPU arch.
    if (telemetryInfo && telemetryInfo.cpuResult &&
        telemetryInfo.cpuResult.cpuInfo) {
      /** @suppress {checkTypes} */
      telemetryInfo.cpuResult.cpuInfo.architecture =
          this.convertCpuArch(telemetryInfo.cpuResult.cpuInfo.architecture);
    }

    // Convert errors.
    if (telemetryInfo && telemetryInfo.batteryResult &&
        telemetryInfo.batteryResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.batteryResult.error.type =
          this.convertErrorType(telemetryInfo.batteryResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.blockDeviceResult &&
        telemetryInfo.blockDeviceResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.blockDeviceResult.error.type =
          this.convertErrorType(telemetryInfo.blockDeviceResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.vpdResult &&
        telemetryInfo.vpdResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.vpdResult.error.type =
          this.convertErrorType(telemetryInfo.vpdResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.cpuResult &&
        telemetryInfo.cpuResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.cpuResult.error.type =
          this.convertErrorType(telemetryInfo.cpuResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.timezoneResult &&
        telemetryInfo.timezoneResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.timezoneResult.error.type =
          this.convertErrorType(telemetryInfo.timezoneResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.memoryResult &&
        telemetryInfo.memoryResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.memoryResult.error.type =
          this.convertErrorType(telemetryInfo.memoryResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.backlightResult &&
        telemetryInfo.backlightResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.backlightResult.error.type =
          this.convertErrorType(telemetryInfo.backlightResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.fanResult &&
        telemetryInfo.fanResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.fanResult.error.type =
          this.convertErrorType(telemetryInfo.fanResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.statefulPartitionResult &&
        telemetryInfo.statefulPartitionResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.statefulPartitionResult.error.type = this.convertErrorType(
          telemetryInfo.statefulPartitionResult.error.type);
    }
    if (telemetryInfo && telemetryInfo.bluetoothResult &&
        telemetryInfo.bluetoothResult.error) {
      /** @suppress {checkTypes} */
      telemetryInfo.bluetoothResult.error.type =
          this.convertErrorType(telemetryInfo.bluetoothResult.error.type);
    }

    return telemetryInfo;
  }

  /**
   * This method converts Mojo types to WebIDL types applying next rules:
   *   1. remove null objects from arrays;
   *   2. convert objects like { value: X } to X, where X is either a number or
   *      a boolean;
   *   3. omit null/undefined properties;
   *   4. convert objects without properties to null.
   * @param {?Object|string|number|null|undefined} input
   * @return {?Object|string|number|null}
   */
  convert(input) {
    // After this closure compiler knows that input is not null.
    if (input === null || typeof input === 'undefined') {
      return null;
    }

    // Rule #1: remove null objects from arrays.
    if (Array.isArray(input)) {
      return input
          .map(
              (/** @type {?Object|string|number|null} */ item) =>
                  this.convert(item))
          .filter(
              (/** @type {!Object|string|number|null} */ item) =>
                  item !== null);
    }

    // After this closure compiler knows that input is {!Object}.
    if (typeof input !== 'object') {
      return input;
    }

    // At this point, closure compiler knows that the input is {!Object}.

    // Rule #2: convert objects like { value: X } to X, where X is either a
    // number or a boolean.
    if (Object.entries(input).length === 1 &&
        (typeof input['value'] === 'number' ||
         typeof input['value'] === 'boolean')) {
      return input['value'];
    }

    let output = {};
    Object.entries(input).forEach(kv => {
      const key = /** @type {!string} */ (kv[0]);
      const value = /** @type {?Object|string|number|null|undefined} */ (kv[1]);
      const convertedValue = this.convert(value);

      // Rule #3: omit null/undefined properties.
      if (convertedValue !== null && typeof convertedValue !== 'undefined') {
        output[key] = convertedValue;
      }
    });

    // Rule #4. convert objects without properties to null.
    if (Object.entries(output).length === 0) {
      return null;
    }
    return output;
  };

  /**
   * Requests telemetry info.
   * @param { Object } message
   * @return { !Promise<!dpsl_internal.ProbeTelemetryInfoResponse> }
   */
  async handleProbeTelemetryInfo(message) {
    const request =
        /** @type {!dpsl_internal.ProbeTelemetryInfoRequest} */ (message);

    /** @type {!Array<!chromeos.health.mojom.ProbeCategoryEnum>} */
    let categories = [];
    try {
      categories = this.convertCategories(request);
    } catch (/** @type {!Error}*/ error) {
      return error;
    }

    const telemetryInfo =
        await getOrCreateProbeService().probeTelemetryInfo(categories);
    return /** @type {!Object} */ (
        this.convert(this.convertAllEnums(telemetryInfo.telemetryInfo)) || {});
  }
};

const telemetryProxy = new TelemetryProxy();

/**
 * Proxying event requests between SystemEventsRequester on
 * chrome-untrusted:// side with WebIDL types and SystemEventsService on
 * chrome:// side with Mojo types.
 */
class SystemEventsProxy {
  /**
   * @param {!MessagePipe} messagePipe
   * @param {!Promise<undefined>} iframeReady
   */
  constructor(messagePipe, iframeReady) {
    this.messagePipe = messagePipe;
    this.iframeReady = iframeReady;

    this.events = {
      ON_LID_CLOSED: 'lid-closed',
      ON_LID_OPENED: 'lid-opened',
    };

    this.lidObserverCallbackRouter_ =
        new chromeos.health.mojom.LidObserverCallbackRouter();

    this.lidObserverCallbackRouter_.onLidClosed.addListener(
        () => this.sendEvent(this.events.ON_LID_CLOSED));
    this.lidObserverCallbackRouter_.onLidOpened.addListener(
        () => this.sendEvent(this.events.ON_LID_OPENED));

    getOrCreateSystemEventsService().addLidObserver(
        this.lidObserverCallbackRouter_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Sends event type to chrome-untrusted://.
   * @param {!string} type
   */
  async sendEvent(type) {
    /* Prevent events from being sent until untrusted iframe is loaded. */
    await this.iframeReady;
    this.messagePipe.sendMessage(
        dpsl_internal.Message.SYSTEM_EVENTS_SERVICE_EVENTS,
        /** @type {!dpsl_internal.Event} */ ({type: type}));
  }
}

const untrustedMessagePipe =
    new MessagePipe('chrome-untrusted://telemetry-extension');

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_AVAILABLE_ROUTINES,
    () => diagnosticsProxy.handleGetAvailableRoutines());

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_ROUTINE_UPDATE,
    (message) => diagnosticsProxy.handleGetRoutineUpdate(message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CAPACITY_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunBatteryCapacityRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_HEALTH_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunBatteryHealthRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_SMARTCTL_CHECK_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        () => diagnosticsProxy.handleRunSmartctlCheckRoutine(), message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_AC_POWER_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunAcPowerRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_CACHE_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunCpuCacheRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_CPU_STRESS_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunCpuStressRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_FP_ACCURACY_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) =>
            diagnosticsProxy.handleRunFloatingPointAccuracyRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_WEAR_LEVEL_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunNvmeWearLevelRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_NVME_SELF_TEST_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunNvmeSelfTestRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_DISK_READ_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunDiskReadRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_PRIME_SEARCH_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunPrimeSearchRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_DISCHARGE_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunBatteryDischargeRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_RUN_BATTERY_CHARGE_ROUTINE,
    (message) => diagnosticsProxy.genericRunRoutineHandler(
        (message) => diagnosticsProxy.handleRunBatteryChargeRoutine(message),
        message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.PROBE_TELEMETRY_INFO,
    (message) => telemetryProxy.handleProbeTelemetryInfo(message));

/**
 * Promise that resolves once the iframe is ready to receive messages.
 * @type {!Promise<undefined>}
 */
const iframeReady = new Promise(resolve => {
  // Wait for 'load' (and not DOMContentLoaded) to ensure the subframe has been
  // loaded and is ready to respond to postMessage.
  window.addEventListener('load', () => {
    resolve();
  });
});

const systemEventsProxy =
    new SystemEventsProxy(untrustedMessagePipe, iframeReady);
