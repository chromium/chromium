// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../../browser_proxy/browser_proxy.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from '../../device/camera3_device_info.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import * as nav from '../../nav.js';
import {PerfEvent} from '../../perf.js';
import * as state from '../../state.js';
import {Facing, ViewName} from '../../type.js';
import * as util from '../../util.js';

/**
 * Creates a controller for the options of Camera view.
 */
export class Options {
  /**
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {function()} doSwitchDevice Callback to trigger device switching.
   */
  constructor(infoUpdater, doSwitchDevice) {
    /**
     * @type {!DeviceInfoUpdater}
     * @private
     * @const
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {function()}
     * @private
     * @const
     */
    this.doSwitchDevice_ = doSwitchDevice;

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMic_ = dom.get('#toggle-mic', HTMLInputElement);

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMirror_ = dom.get('#toggle-mirror', HTMLInputElement);

    /**
     * Device id of the camera device currently used or selected.
     * @type {?string}
     * @private
     */
    this.videoDeviceId_ = null;

    /**
     * Whether list of video devices is being refreshed now.
     * @type {boolean}
     * @private
     */
    this.refreshingVideoDeviceIds_ = false;

    /**
     * Whether the current device is HALv1 and lacks facing configuration.
     * get facing information.
     * @type {?boolean}
     * private
     */
    this.isV1NoFacingConfig_ = null;

    /**
     * Mirroring set per device.
     * @type {!Object}
     * @private
     */
    this.mirroringToggles_ = {};

    /**
     * Current audio track in use.
     * @type {?MediaStreamTrack}
     * @private
     */
    this.audioTrack_ = null;

    [['#switch-device', () => this.switchDevice_()],
     ['#toggle-grid', () => this.animatePreviewGrid_()],
     ['#open-settings', () => nav.open(ViewName.SETTINGS)],
    ]
        .forEach(
            ([selector, fn]) =>
                document.querySelector(selector).addEventListener('click', fn));

    this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
    this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

    // Restore saved mirroring states per video device.
    browserProxy.localStorageGet({mirroringToggles: {}})
        .then((values) => this.mirroringToggles_ = values['mirroringToggles']);
    // Remove the deprecated values.
    browserProxy.localStorageRemove(
        ['effectIndex', 'toggleMulti', 'toggleMirror']);

    this.infoUpdater_.addDeviceChangeListener(async (updater) => {
      state.set(
          state.State.MULTI_CAMERA,
          (await updater.getDevicesInfo()).length >= 2);
    });
  }

  /**
   * Device id of the camera device currently used or selected.
   * @return {?string}
   */
  get currentDeviceId() {
    return this.videoDeviceId_;
  }

  /**
   * Switches to the next available camera device.
   * @private
   */
  async switchDevice_() {
    if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return;
    }
    state.set(PerfEvent.CAMERA_SWITCHING, true);
    const devices = await this.infoUpdater_.getDevicesInfo();
    util.animateOnce(dom.get('#switch-device', HTMLElement));
    let index =
        devices.findIndex((entry) => entry.deviceId === this.videoDeviceId_);
    if (index === -1) {
      index = 0;
    }
    if (devices.length > 0) {
      index = (index + 1) % devices.length;
      this.videoDeviceId_ = devices[index].deviceId;
    }
    const isSuccess = await this.doSwitchDevice_();
    state.set(PerfEvent.CAMERA_SWITCHING, false, {hasError: !isSuccess});
  }

  /**
   * Animates the preview grid.
   * @private
   */
  animatePreviewGrid_() {
    Array.from(document.querySelector('#preview-grid').children)
        .forEach((grid) => util.animateOnce(grid));
  }

  /**
   * Maps MediaTrackSettings.facingMode to CCA facing type.
   * @param {string|undefined} facing The target facingMode to map.
   * @return {!Facing} The mapped CCA facing.
   * @private
   */
  mapFacing_(facing) {
    switch (facing) {
      case undefined:
        return Facing.EXTERNAL;
      case 'user':
        return Facing.USER;
      case 'environment':
        return Facing.ENVIRONMENT;
      default:
        throw new Error('Unknown facing: ' + facing);
    }
  }

  /**
   * Updates the options' values for the current constraints and stream.
   * @param {!MediaStream} stream Current Stream in use.
   * @return {!Promise<!Facing>} Facing-mode in use.
   */
  async updateValues(stream) {
    const track = stream.getVideoTracks()[0];
    const trackSettings = track.getSettings && track.getSettings();
    const facingMode = trackSettings && trackSettings.facingMode;
    if (this.isV1NoFacingConfig_ === null) {
      // Because the facing mode of external camera will be set to undefined on
      // all devices, to distinguish HALv1 device without facing configuration,
      // assume the first opened camera is built-in camera. Device without
      // facing configuration won't set facing of built-in cameras. Also if
      // HALv1 device with facing configuration opened external camera first
      // after CCA launched the logic here may misjudge it as this category.
      this.isV1NoFacingConfig_ = facingMode === undefined;
    }
    const facing =
        this.isV1NoFacingConfig_ ? Facing.NOT_SET : this.mapFacing_(facingMode);
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
    this.updateMirroring_(facing);
    this.audioTrack_ = stream.getAudioTracks()[0];
    this.updateAudioByMic_();
    return facing;
  }

  /**
   * Updates mirroring for a new stream.
   * @param {!Facing} facing Facing of the stream.
   * @private
   */
  updateMirroring_(facing) {
    // Update mirroring by detected facing-mode. Enable mirroring by default if
    // facing-mode isn't available.
    let enabled = facing !== Facing.ENVIRONMENT;

    // Override mirroring only if mirroring was toggled manually.
    if (this.videoDeviceId_ in this.mirroringToggles_) {
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    }

    util.toggleChecked(this.toggleMirror_, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   * @private
   */
  saveMirroring_() {
    this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
    browserProxy.localStorageSet({mirroringToggles: this.mirroringToggles_});
  }

  /**
   * Enables/disables the current audio track by the microphone option.
   * @private
   */
  updateAudioByMic_() {
    if (this.audioTrack_) {
      this.audioTrack_.enabled = this.toggleMic_.checked;
    }
  }

  /**
   * Gets the video device ids sorted by preference.
   * @return {!Promise<!Array<?string>>} May contain null for user facing camera
   *     on HALv1 devices.
   */
  async videoDeviceIds() {
    /** @type {!Array<(!Camera3DeviceInfo|!MediaDeviceInfo)>} */
    let devices;
    /**
     * Object mapping from device id to facing. Set to null on HALv1 device.
     * @type {?Object<string, !Facing>}
     */
    let facings = null;

    const camera3Info = await this.infoUpdater_.getCamera3DevicesInfo();
    if (camera3Info) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
      this.isV1NoFacingConfig_ = false;
    } else {
      devices = await this.infoUpdater_.getDevicesInfo();
    }

    const defaultFacing = util.getDefaultFacing();
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.videoDeviceId_ ? a === this.videoDeviceId_ :
                                (facings && facings[a] === defaultFacing)) {
        return -1;
      }
      return 1;
    });
    // Prepended 'null' deviceId means the system default camera on HALv1
    // device. Add it only when the app is launched (no video-device-id set).
    if (!facings && this.videoDeviceId_ === null) {
      sorted.unshift(null);
    }
    return sorted;
  }
}
