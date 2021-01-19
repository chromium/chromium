// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {assertString} from '../chrome_util.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {
  Facing,
  VideoConfig,  // eslint-disable-line no-unused-vars
} from '../type.js';
import {Camera3DeviceInfo} from './camera3_device_info.js';

/**
 * The singleton instance of StreamManager. Initialized by the first
 * invocation of getInstance().
 * @type {?StreamManager}
 */
let instance = null;

/**
 * Device information includs MediaDeviceInfo and Camera3DeviceInfo.
 * @typedef {{
 *   v1Info: !MediaDeviceInfo,
 *   v3Info: !Camera3DeviceInfo
 * }}
 */
export let DeviceInfo;

/**
 * Creates extra stream for the current mode.
 */
export class CaptureStream {
  /**
   * @param {string} deviceId Device id of currently working video device
   * @param {!MediaStream} stream Capture stream
   * @private
   */
  constructor(deviceId, stream) {
    /**
     * Device id of currently working video device.
     * @type {string}
     * @private
     */
    this.deviceId_ = deviceId;

    /**
     * Capture stream
     * @type {!MediaStream}
     * @protected
     */
    this.stream_ = stream;
  }

  /**
   * @return {!MediaStream}
   * @public
   */
  get stream() {
    return this.stream_;
  }

  /**
   * Closes stream
   * @public
   */
  async close() {
    this.stream_.getVideoTracks()[0].stop();
    try {
      await StreamManager.getInstance().setMultipleStreamsEnabled(
          this.deviceId_, false);
    } catch (e) {
      console.error(e);
    }
  }
}

/**
 * Monitors device change and provides different listener callbacks for
 * device changes. It also provides streams for different modes.
 */
export class StreamManager {
  /**
   * @private
   */
  constructor() {
    /**
     * MediaDeviceInfo of all available video devices.
     * @type {?Promise<!Array<!MediaDeviceInfo>>}
     * @private
     */
    this.devicesInfo_ = null;

    /**
     * Got the permission to run enumerateDevices() or not.
     * @type {boolean}
     * @private
     */
    this.canEnumerateDevices_ = false;

    /**
     * Camera3DeviceInfo of all available video devices. Is null on HALv1 device
     * without mojo api support.
     * @type {?Promise<?Array<!DeviceInfo>>}
     * @private
     */
    this.camera3DevicesInfo_ = null;

    /**
     * Listeners for real device change event.
     * @type {!Array<function(!Array<!DeviceInfo>): !Promise>}
     * @private
     */
    this.realListeners_ = [];

    /**
     * Latest result of Camera3DeviceInfo of all real video devices.
     * @type {!Array<!DeviceInfo>}
     * @private
     */
    this.realDevices_ = [];

    /**
     * Maps from real device id to corresponding virtual devices id and it is
     * only available on HALv3.
     * @type {!Map<string, string>}
     * @private
     */
    this.virtualMap_ = new Map();

    /**
     * Filter out lagging 720p on grunt. See https://crbug.com/1122852.
     * @const {!Promise<function(!VideoConfig): boolean>}
     * @private
     */
    this.videoConfigFilter_ = (async () => {
      const board = await browserProxy.getBoard();
      return board === 'grunt' ? ({height}) => height < 720 : () => true;
    })();

    navigator.mediaDevices.addEventListener(
        'devicechange', this.deviceUpdate.bind(this));
  }

  /**
   * Creates a new instance of StreamManager if it is not set. Returns the
   *     exist instance.
   * @return {!StreamManager} The singleton instance.
   */
  static getInstance() {
    if (instance === null) {
      instance = new StreamManager();
    }
    return instance;
  }

  /**
   * Registers listener to be called when state of available real devices
   * changes.
   * @param {function(!Array<!DeviceInfo>)} listener
   */
  addRealDeviceChangeListener(listener) {
    this.realListeners_.push(listener);
  }

  /**
   * Creates extra stream according to the constraints.
   * @param {!MediaStreamConstraints} constraints
   * @return {!Promise<!CaptureStream>}
   */
  async openCaptureStream(constraints) {
    const realDeviceId = assertString(constraints.video.deviceId.exact);
    try {
      await this.setMultipleStreamsEnabled(realDeviceId, true);
    } catch (e) {
      console.error(e);
    }

    constraints.video.deviceId.exact = this.virtualMap_.get(realDeviceId);

    const stream = await navigator.mediaDevices.getUserMedia(constraints);
    return new CaptureStream(realDeviceId, stream);
  }

  /**
   * Handling function for device changing.
   */
  async deviceUpdate() {
    const devices = await this.doDeviceInfoUpdate_();
    if (devices === null) {
      return;
    }
    await this.doDeviceNotify_(devices);
  }

  /**
   * Gets devices information via mojo IPC.
   * @return {?Promise<?Array<!DeviceInfo>>}
   * @private
   */
  async doDeviceInfoUpdate_() {
    this.devicesInfo_ = this.enumerateDevices_();
    this.camera3DevicesInfo_ = this.queryMojoDevicesInfo_();
    try {
      return await this.camera3DevicesInfo_;
    } catch (e) {
      console.error(e);
    }
    return null;
  }

  /**
   * Notifies device changes to listeners and create a mapping for real and
   * virtual device.
   * @param {!Array<!DeviceInfo>} devices
   * @private
   */
  async doDeviceNotify_(devices) {
    const isVirtual = (d) => d.v1Info.label.startsWith('Virtual Camera');
    const realDevices = devices.filter((d) => !isVirtual(d));
    const virtualDevices = devices.filter(isVirtual);
    if (this.isRealDeviceChange_(realDevices)) {
      await Promise.all(this.realListeners_.map((l) => l(realDevices)));
    }
    this.virtualMap_ = new Map();
    for (const device of virtualDevices) {
      let dev = null;
      switch (device.v3Info.facing) {
        case Facing.VIRTUAL_USER:
          dev = realDevices.find((d) => d.v3Info.facing === Facing.USER);
          break;
        case Facing.VIRTUAL_ENV:
          dev = realDevices.find((d) => d.v3Info.facing === Facing.ENVIRONMENT);
          break;
        case Facing.VIRTUAL_EXT:
          // TODO(b/172306905): Handle multiple external devices.
          dev = realDevices.find((d) => d.v3Info.facing === Facing.EXTERNAL);
          break;
        default:
          console.error(`Invalid facing: ${device.v3Info.facing}`);
      }
      if (dev) {
        this.virtualMap_.set(dev.v3Info.deviceId, device.v3Info.deviceId);
      }
    }
    this.realDevices_ = realDevices;
  }

  /**
   * Compares devices info to see whether there are only real devices change.
   * @param {!Array<!DeviceInfo>} devices
   * @return {boolean}
   * @private
   */
  isRealDeviceChange_(devices) {
    if (this.realDevices_.length === 0) {
      return true;
    }
    const realChange = (devices1, devices2) => {
      for (const device of devices1) {
        const found =
            devices2.find(
                (d2) => d2.v3Info.deviceId === device.v3Info.deviceId) ||
            null;
        if (found === null) {
          return true;
        }
      }
      return false;
    };
    return realChange(devices, this.realDevices_) ||
        realChange(this.realDevices_, devices);
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
   * @return {!Promise<?Array<!DeviceInfo>>} Camera3DeviceInfo of available
   *     devices. Maybe null on HALv1 devices without supporting private mojo
   *     api.
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
    return Promise.all(deviceInfos.map(
        async (d) => ({
          v1Info: d,
          v3Info: await Camera3DeviceInfo.create(d, videoConfigFilter),
        })));
  }

  /**
   * Enables/Disables multiple streams on target camera device. The extra
   * stream will be reported as virtual video device from
   * navigator.mediaDevices.enumerateDevices().
   * @param {string} deviceId The id of target camera device.
   * @param {boolean} enabled True for eanbling multiple streams.
   */
  async setMultipleStreamsEnabled(deviceId, enabled) {
    const deviceOperator = await DeviceOperator.getInstance();
    await deviceOperator.setMultipleStreamsEnabled(deviceId, enabled);
    await this.deviceUpdate();
    if (this.virtualMap_.has(deviceId) !== enabled) {
      throw new Error(`${deviceId} set multiple streams to ${enabled} failed`);
    }
  }
}
