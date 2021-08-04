// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bitmapToJpegBlob} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {DeviceOperator} from './device_operator.js';
import {closeEndpoint} from './util.js';

/**
 * Extended PhotoCapabilities type used by Chrome OS HAL3 VCD.
 * @extends {PhotoCapabilities}
 * @record
 */
export class CrosPhotoCapabilities {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!Array<string>}
     */
    this.supportedEffects;
  }
}

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 */
export class CrosImageCapture {
  /**
   * @param {!MediaStreamTrack} videoTrack A video track whose still images will
   *     be taken.
   */
  constructor(videoTrack) {
    /**
     * The id of target media device.
     * @type {string}
     * @private
     */
    this.deviceId_ = videoTrack.getSettings().deviceId;

    /**
     * The standard ImageCapture object.
     * @type {!ImageCapture}
     * @private
     */
    this.capture_ = new ImageCapture(videoTrack);
  }

  /**
   * Gets the photo capabilities with the available options/effects.
   * @return {!Promise<!PhotoCapabilities|!CrosPhotoCapabilities>} Promise
   *     for the result.
   */
  async getPhotoCapabilities() {
    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return this.capture_.getPhotoCapabilities();
    }

    const supportedEffects = [cros.mojom.Effect.NO_EFFECT];
    const isPortraitModeSupported =
        await deviceOperator.isPortraitModeSupported(this.deviceId_);
    if (isPortraitModeSupported) {
      supportedEffects.push(cros.mojom.Effect.PORTRAIT_MODE);
    }
    const baseCapabilities = await this.capture_.getPhotoCapabilities();

    const extendedCapabilities = /** @type{!CrosPhotoCapabilities} */ (
        Object.assign({}, baseCapabilities, {supportedEffects}));
    return extendedCapabilities;
  }

  /**
   * Takes single or multiple photo(s) with the specified settings and effects.
   * The amount of result photo(s) depends on the specified settings and
   * effects, and the first promise in the returned array will always resolve
   * with the unreprocessed photo. The returned array will be resolved once it
   * received the shutter event.
   * @param {!PhotoSettings} photoSettings Photo settings for ImageCapture's
   *     takePhoto().
   * @param {!Array<!cros.mojom.Effect>=} photoEffects Photo effects to be
   *     applied.
   * @return {!Promise<!Array<!Promise<!Blob>>>} A promise of the array
   *     containing promise of each blob result.
   */
  async takePhoto(photoSettings, photoEffects = []) {
    /** @type {!Array<!Promise<!Blob>>} */
    const takes = [];
    const deviceOperator = await DeviceOperator.getInstance();
    if (deviceOperator === null && photoEffects.length > 0) {
      throw new Error('Applying effects is not supported on this device');
    }

    for (const effect of photoEffects) {
      const take = (async () => {
        const {data, mimeType} =
            await deviceOperator.setReprocessOption(this.deviceId_, effect);
        return new Blob([new Uint8Array(data)], {type: mimeType});
      })();
      takes.push(take);
    }

    if (deviceOperator !== null) {
      const onShutterDone = new WaitableEvent();
      const shutterObserver =
          await deviceOperator.addShutterObserver(this.deviceId_, () => {
            onShutterDone.signal();
          });
      takes.unshift(this.capture_.takePhoto(photoSettings));
      await onShutterDone.wait();
      closeEndpoint(shutterObserver);
      return takes;
    } else {
      takes.unshift(this.capture_.takePhoto(photoSettings));
      return takes;
    }
  }

  /**
   * @return {!Promise<!ImageBitmap>}
   */
  grabFrame() {
    return this.capture_.grabFrame();
  }

  /**
   * @return {!Promise<!Blob>} Returns jpeg blob of the grabbed frame.
   */
  async grabJpegFrame() {
    const bitmap = await this.capture_.grabFrame();
    return bitmapToJpegBlob(bitmap);
  }
}
