// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {DeviceOperator} from '../mojo/device_operator.js';
// eslint-disable-next-line no-unused-vars
import {ResolutionList, VideoConfig} from '../type.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from './constraints_preferrer.js';

/**
 * Contains information of all cameras on the device and will updates its value
 * when any plugin/unplug external camera changes.
 */
export class DeviceInfoUpdater {
  /**
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @public
   * */
  constructor(photoPreferrer, videoPreferrer) {
    /**
     * @type {!PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = photoPreferrer;

    /**
     * @type {!VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = videoPreferrer;

    /**
     * Listeners to be called after new camera information is available.
     * @type {!Array<function(!DeviceInfoUpdater): !Promise>}
     * @private
     */
    this.deviceChangeListeners_ = [];

    /**
     * Action locking update of camera information.
     * @type {?Promise}
     * @private
     */
    this.lockingUpdate_ = null;

    /**
     * Pending camera information update while update capability is locked.
     * @type {?Promise}
     * @private
     */
    this.pendingUpdate_ = null;

    /**
     * MediaDeviceInfo of all available video devices.
     * @type {!Promise<!Array<!MediaDeviceInfo>>}
     * @private
     */
    this.devicesInfo_ = this.enumerateDevices_();

    /**
     * Got the permission to run enumerateDevices() or not.
     * @type {boolean}
     * @private
     */
    this.canEnumerateDevices_ = false;

    /**
     * Camera3DeviceInfo of all available video devices. Is null on HALv1 device
     * without mojo api support.
     * @type {!Promise<?Array<!Camera3DeviceInfo>>}
     * @private
     */
    this.camera3DevicesInfo_ = this.queryMojoDevicesInfo_();

    /**
     * Filter out lagging 720p on grunt. See https://crbug.com/1122852.
     * @const {!Promise<function(!VideoConfig): boolean>}
     * @private
     */
    this.videoConfigFilter_ = (async () => {
      const board = await browserProxy.getBoard();
      return board === 'grunt' ? ({height}) => height < 720 : () => true;
    })();

    /**
     * Promise of first update.
     * @type {!Promise}
     */
    this.firstUpdate_ = this.update_();

    navigator.mediaDevices.addEventListener(
        'devicechange', this.update_.bind(this));
  }

  /**
   * Tries to gain lock and initiates update process.
   * @private
   */
  async update_() {
    if (this.lockingUpdate_) {
      if (this.pendingUpdate_) {
        return;
      }
      this.pendingUpdate_ = (async () => {
        while (this.lockingUpdate_) {
          try {
            await this.lockingUpdate_;
          } catch (e) {
            // Ignore exception from waiting for existing update.
          }
        }
        this.lockingUpdate_ = this.pendingUpdate_;
        this.pendingUpdate_ = null;
        await this.doUpdate_();
        this.lockingUpdate_ = null;
      })();
    } else {
      this.lockingUpdate_ = (async () => {
        await this.doUpdate_();
        this.lockingUpdate_ = null;
      })();
    }
  }

  /**
   * Updates devices information.
   * @private
   */
  async doUpdate_() {
    this.devicesInfo_ = this.enumerateDevices_();
    this.camera3DevicesInfo_ = this.queryMojoDevicesInfo_();
    try {
      await this.devicesInfo_;
      const devices = await this.camera3DevicesInfo_;
      if (devices) {
        this.photoPreferrer_.updateDevicesInfo(devices);
        this.videoPreferrer_.updateDevicesInfo(devices);
      }
      await Promise.all(this.deviceChangeListeners_.map((l) => l(this)));
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Enumerates all available devices and gets their MediaDeviceInfo.
   * @return {!Promise<!Array<!MediaDeviceInfo>>}
   * @throws {!Error}
   * @private
   */
  async enumerateDevices_() {
    if (!this.canEnumerateDevices_) {
      this.canEnumerateDevices_ =
          await browserProxy.requestEnumerateDevicesPermission();
      if (!this.canEnumerateDevices_) {
        throw new Error('Failed to get the permission for enumerateDevices()');
      }
    }
    const devices = (await navigator.mediaDevices.enumerateDevices())
                        .filter((device) => device.kind === 'videoinput');
    if (devices.length === 0) {
      throw new Error('Device list empty.');
    }
    return devices;
  }

  /**
   * Queries Camera3DeviceInfo of available devices through private mojo API.
   * @return {!Promise<?Array<!Camera3DeviceInfo>>} Camera3DeviceInfo
   *     of available devices. Maybe null on HALv1 devices without supporting
   *     private mojo api.
   * @throws {!Error} Thrown when camera unplugging happens between enumerating
   *     devices and querying mojo APIs with current device info results.
   * @private
   */
  async queryMojoDevicesInfo_() {
    if (!await DeviceOperator.isSupported()) {
      return null;
    }
    const deviceInfos = await this.devicesInfo_;
    const videoConfigFilter = await this.videoConfigFilter_;
    return Promise.all(
        deviceInfos.map((d) => Camera3DeviceInfo.create(d, videoConfigFilter)));
  }

  /**
   * Registers listener to be called when state of available devices changes.
   * @param {function(!DeviceInfoUpdater)} listener
   */
  addDeviceChangeListener(listener) {
    this.deviceChangeListeners_.push(listener);
  }

  /**
   * Requests to lock update of device information. This function is preserved
   * for device information reader to lock the update capability so as to ensure
   * getting consistent data between all information providers.
   * @param {function(): !Promise} callback Called after
   *     update capability is locked. Getting information from all providers in
   *     callback are guaranteed to be consistent.
   */
  async lockDeviceInfo(callback) {
    await this.firstUpdate_;
    while (this.lockingUpdate_ || this.pendingUpdate_) {
      try {
        await this.lockingUpdate_;
        await this.pendingUpdate_;
      } catch (e) {
        // Ignore exception from waiting for existing update.
      }
    }
    this.lockingUpdate_ = (async () => {
      try {
        await callback();
      } finally {
        this.lockingUpdate_ = null;
      }
    })();
    await this.lockingUpdate_;
  }

  /**
   * Gets MediaDeviceInfo for all available video devices.
   * @return {!Promise<!Array<!MediaDeviceInfo>>}
   */
  async getDevicesInfo() {
    return this.devicesInfo_;
  }

  /**
   * Gets MediaDeviceInfo of specific video device.
   * @param {string} deviceId Device id of video device to get information from.
   * @return {!Promise<?MediaDeviceInfo>}
   */
  async getDeviceInfo(deviceId) {
    const /** !Array<!MediaDeviceInfo> */ infos = await this.getDevicesInfo();
    return infos.find((d) => d.deviceId === deviceId) || null;
  }

  /**
   * Gets Camera3DeviceInfo for all available video devices.
   * @return {!Promise<?Array<!Camera3DeviceInfo>>}
   */
  async getCamera3DevicesInfo() {
    return this.camera3DevicesInfo_;
  }
}
