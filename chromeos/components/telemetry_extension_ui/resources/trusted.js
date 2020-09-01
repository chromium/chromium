// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let diagnosticsService = null;
let probeService = null;

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
      [routineEnum.kUrandom, 'urandom'],
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
   * This method converts Mojo types to WebIDL types applying next rules:
   *   1. remove null objects from arrays;
   *   2. convert objects like { value: X } to X, where X is a number;
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

    // Rule #2: convert objects like { value: X } to X, where X is a number.
    if (Object.entries(input).length === 1 &&
        typeof input['value'] === 'number') {
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
        this.convert(telemetryInfo.telemetryInfo) || {});
  }
};

const telemetryProxy = new TelemetryProxy();

const untrustedMessagePipe =
    new MessagePipe('chrome-untrusted://telemetry-extension');

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_AVAILABLE_ROUTINES,
    () => diagnosticsProxy.handleGetAvailableRoutines());

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.DIAGNOSTICS_ROUTINE_UPDATE,
    (message) => diagnosticsProxy.handleGetRoutineUpdate(message));

untrustedMessagePipe.registerHandler(
    dpsl_internal.Message.PROBE_TELEMETRY_INFO,
    (message) => telemetryProxy.handleProbeTelemetryInfo(message));
