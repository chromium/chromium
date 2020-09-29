// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Filenamer} from '../../../models/file_namer.js';
import * as filesystem from '../../../models/file_system.js';
import {DeviceOperator, parseMetadata} from '../../../mojo/device_operator.js';
import {CrosImageCapture} from '../../../mojo/image_capture.js';
import {PerfEvent} from '../../../perf.js';
import * as state from '../../../state.js';
import * as toast from '../../../toast.js';
import {
  Facing,  // eslint-disable-line no-unused-vars
  Resolution,
} from '../../../type.js';
import * as util from '../../../util.js';

import {ModeBase} from './mode_base.js';

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: !Resolution,
 *     blob: !Blob,
 *     isVideoSnapshot: (boolean|undefined),
 * }}
 */
export let PhotoResult;

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class PhotoHandler {
  /**
   * Handles the result photo.
   * @param {!PhotoResult} photo Captured photo result.
   * @param {string} name Name of the photo result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultPhoto(photo, name) {}

  /**
   * Plays UI effect when taking photo.
   */
  playShutterEffect() {}
}


/**
 * Photo mode capture controller.
 */
export class Photo extends ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution
   * @param {!PhotoHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution);

    /**
     * @const {!PhotoHandler}
     * @protected
     */
    this.handler_ = handler;

    /**
     * CrosImageCapture object to capture still photos.
     * @type {?CrosImageCapture}
     * @protected
     */
    this.crosImageCapture_ = null;

    /**
     * The observer id for saving metadata.
     * @type {?number}
     * @protected
     */
    this.metadataObserverId_ = null;

    /**
     * Metadata names ready to be saved.
     * @type {!Array<string>}
     * @protected
     */
    this.metadataNames_ = [];
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new CrosImageCapture(this.stream_.getVideoTracks()[0]);
    }

    await this.takePhoto_();
  }

  /**
   * Takes and saves a photo.
   * @return {!Promise}
   * @private
   */
  async takePhoto_() {
    const imageName = (new Filenamer()).newImageName();
    if (this.metadataObserverId_ !== null) {
      this.metadataNames_.push(Filenamer.getMetadataName(imageName));
    }

    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    try {
      const results = await this.crosImageCapture_.takePhoto(photoSettings);

      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {facing: this.facing_});
      this.handler_.playShutterEffect();

      state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);
      const blob = await results[0];
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      await this.handler_.handleResultPhoto({resolution, blob}, imageName);
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing_});
    } catch (e) {
      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {hasError: true});
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  /**
   * Adds an observer to save metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {
    if (!this.stream_) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const cameraMetadataTagInverseLookup = {};
    Object.entries(cros.mojom.CameraMetadataTag).forEach(([key, value]) => {
      if (key === 'MIN_VALUE' || key === 'MAX_VALUE') {
        return;
      }
      cameraMetadataTagInverseLookup[value] = key;
    });

    const callback = (metadata) => {
      const parsedMetadata = {};
      for (const entry of metadata.entries) {
        const key = cameraMetadataTagInverseLookup[entry.tag];
        if (key === undefined) {
          // TODO(kaihsien): Add support for vendor tags.
          continue;
        }

        const val = parseMetadata(entry);
        parsedMetadata[key] = val;
      }

      filesystem.saveBlob(
          new Blob(
              [JSON.stringify(parsedMetadata, null, 2)],
              {type: 'application/json'}),
          this.metadataNames_.shift());
    };

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, cros.mojom.StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {
    if (!this.stream_ || this.metadataObserverId_ === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    const isSuccess = await deviceOperator.removeMetadataObserver(
        deviceId, this.metadataObserverId_);
    if (!isSuccess) {
      console.error(`Failed to remove metadata observer with id: ${
          this.metadataObserverId_}`);
    }
    this.metadataObserverId_ = null;
  }
}
