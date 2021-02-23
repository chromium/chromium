// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Telemetry interface exposed to third-parties for getting device telemetry
 * information.
 */

(() => {
  const messagePipe = dpsl.internal.messagePipe;

  /**
   * Requests telemetry info specified in |categories|.
   * @param { !Array<!string> } categories
   * @return { !Promise<!dpsl_internal.ProbeTelemetryInfoResponse> }
   * @package
   */
  async function getSelectedTelemetryInfo(categories) {
    const response = /** @type {dpsl_internal.ProbeTelemetryInfoResponse} */ (
        await messagePipe.sendMessage(
            dpsl_internal.Message.PROBE_TELEMETRY_INFO, categories));
    if (response instanceof Error) {
      throw response;
    }
    return response;
  }

  /**
   * DPSL Telemetry Requester used in dpsl.telemetry.*.
   */
  class DPSLTelemetryRequester {
    /**
     * Requests telemetry info specified in |category|. Then, parses the
     * response as response.$resultName.$infoName or throw error.
     * @param { !string } category
     * @param { !string } resultName
     * @param { !string } infoName
     * @return { !Promise<!dpsl.TelemetryInfoTypes> }
     * @suppress {checkTypes}
     * @throws { !Error } if the response is error/null/undefined/empty.
     * @private
     */
    async _getSelectedTelemetryInfo(category, resultName, infoName) {
      const response = await getSelectedTelemetryInfo([category]);
      let result = null;
      if (response) {
        result = /** @type {Object} */ (response[resultName]);
      }
      if (result) {
        if ('error' in result) {
          throw new Error(JSON.stringify(result['error']));
        }

        const info = /** @type {Object} */ (result[infoName]);
        if (info && info !== {}) {
          return /** @type {dpsl.TelemetryInfoTypes} */ (info);
        }
      }

      // The response or its inner fields are null/undefined/empty, throw error.
      throw new Error(JSON.stringify(
          {type: 'no-result-error', msg: 'Backend returned no result'}));
    }

    /**
     * Requests Backlight info.
     * @return { !Promise<!dpsl.BacklightInfo> }
     * @public
     */
    async getBacklightInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'backlight', 'backlightResult', 'backlightInfo');
      return /** @type {!dpsl.BacklightInfo} */ (result);
    }

    /**
     * Requests Battery info.
     * @return { !Promise<!dpsl.BatteryInfo> }
     * @public
     */
    async getBatteryInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'battery', 'batteryResult', 'batteryInfo');
      return /** @type {dpsl.BatteryInfo} */ (result);
    }

    /**
     * Requests Bluetooth info.
     * @return { !Promise<!dpsl.BluetoothInfo> }
     * @public
     */
    async getBluetoothInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'bluetooth', 'bluetoothResult', 'bluetoothAdapterInfo');
      return /** @type {dpsl.BluetoothInfo} */ (result);
    }

    /**
     * Requests CachedVpd info.
     * @return { !Promise<!dpsl.VpdInfo> }
     * @public
     */
    async getCachedVpdInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'cached-vpd-data', 'vpdResult', 'vpdInfo');
      return /**  @type {dpsl.VpdInfo} */ (result);
    }

    /**
     * Requests CPU info.
     * @return { !Promise<!dpsl.CpuInfo> }
     * @public
     */
    async getCpuInfo() {
      const result =
          await this._getSelectedTelemetryInfo('cpu', 'cpuResult', 'cpuInfo');
      return /**  @type {dpsl.CpuInfo} */ (result);
    }

    /**
     * Requests Fan info.
     * @return { !Promise<!dpsl.FanInfo> }
     * @public
     */
    async getFanInfo() {
      const result =
          await this._getSelectedTelemetryInfo('fan', 'fanResult', 'fanInfo');
      return /**  @type {dpsl.FanInfo} */ (result);
    }

    /**
     * Requests Memory info.
     * @return { !Promise<!dpsl.MemoryInfo> }
     * @public
     */
    async getMemoryInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'memory', 'memoryResult', 'memoryInfo');
      return /**  @type {dpsl.MemoryInfo} */ (result);
    }

    /**
     * Requests NonRemovableBlockDevice info.
     * @return { !Promise<!dpsl.BlockDeviceInfo> }
     * @public
     */
    async getNonRemovableBlockDevicesInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'non-removable-block-devices', 'blockDeviceResult',
          'blockDeviceInfo');
      return /**  @type {dpsl.BlockDeviceInfo} */ (result);
    }

    /**
     * Requests StatefulPartition info.
     * @return { !Promise<!dpsl.StatefulPartitionInfo> }
     * @public
     */
    async getStatefulPartitionInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'stateful-partition', 'statefulPartitionResult', 'partitionInfo');
      return /**  @type {dpsl.StatefulPartitionInfo} */ (result);
    }

    /**
     * Requests Timezone info.
     * @return { !Promise<!dpsl.TimezoneInfo> }
     * @public
     */
    async getTimezoneInfo() {
      const result = await this._getSelectedTelemetryInfo(
          'timezone', 'timezoneResult', 'timezoneInfo');
      return /**  @type {dpsl.TimezoneInfo} */ (result);
    }
  }

  globalThis.dpsl.telemetry = new DPSLTelemetryRequester();

  /**
   * DPSL Telemetry Requester used in chromeos.teletmery.*.
   * @suppress {checkTypes}
   */
  class TelemetryRequester extends EventTarget {
    constructor() {
      super();
      messagePipe.registerHandler(
        dpsl_internal.Message.SYSTEM_EVENTS_SERVICE_EVENTS, (message) => {
          const event = /** @type {!dpsl_internal.Event} */ (message);
          this.dispatchEvent(new Event(event.type));
        });
    }

    /**
     * Requests telemetry info.
     * @param { !Array<!string> } categories
     * @return { !Object }
     * @public
     */
    async probeTelemetryInfo(categories) {
      console.warn(
        'chromeos.telemetry.probeTelemetryInfo API is deprecated and will be',
        'removed. Use dpsl.telemetry.get*, instead');
      return getSelectedTelemetryInfo(categories);
    }
  };

  globalThis.chromeos.telemetry = new TelemetryRequester();
})();
