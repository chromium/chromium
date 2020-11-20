// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../../../chrome_util.js';
// eslint-disable-next-line no-unused-vars
import {DeviceOperator} from '../../../mojo/device_operator.js';
import {
  Facing,      // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

/**
 * Base class for controlling capture sequence in different camera modes.
 * @abstract
 */
export class ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   */
  constructor(stream, facing, captureResolution) {
    /**
     * Stream of current mode.
     * @type {!MediaStream}
     * @protected
     */
    this.stream_ = stream;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @protected
     */
    this.facing_ = facing;

    /**
     * Capture resolution. May be null on device not support of setting
     * resolution.
     * @type {?Resolution}
     * @protected
     */
    this.captureResolution_ = captureResolution;

    /**
     * Promise for ongoing capture operation.
     * @type {?Promise}
     * @private
     */
    this.capture_ = null;
  }

  /**
   * Initiates video/photo capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  startCapture() {
    if (this.capture_ === null) {
      this.capture_ = this.start_().finally(() => this.capture_ = null);
    }
    return this.capture_;
  }

  /**
   * Stops the ongoing capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  async stopCapture() {
    this.stop_();
    return await this.capture_;
  }

  /**
   * Adds an observer to save image metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {}

  /**
   * Remove the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {}

  /**
   * Initiates video/photo capture operation under this mode.
   * @return {!Promise}
   * @protected
   * @abstract
   */
  async start_() {}

  /**
   * Stops the ongoing capture operation under this mode.
   * @protected
   */
  stop_() {}
}

/**
 * @abstract
 */
export class ModeFactory {
  /**
   * @public
   */
  constructor() {
    /**
     * Preview stream.
     * @type {?MediaStream}
     * @protected
     */
    this.stream_ = null;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @protected
     */
    this.facing_ = Facing.UNKNOWN;

    /**
     * Capture resolution.
     * @type {?Resolution}
     * @protected
     */
    this.captureResolution_ = null;
  }

  /**
   * @return {!MediaStream}
   * @protected
   */
  get previewStream_() {
    return assertInstanceof(this.stream_, MediaStream);
  }

  /**
   * @param {!Resolution} resolution
   */
  setCaptureResolution(resolution) {
    this.captureResolution_ = resolution;
  }

  /**
   * @param {!Facing} facing
   */
  setFacing(facing) {
    this.facing_ = facing;
  }

  /**
   * @param {!MediaStream} stream
   */
  setPreviewStream(stream) {
    this.stream_ = stream;
  }

  /**
   * Makes video capture device prepared for capturing in this mode.
   * @param {!DeviceOperator} deviceOperator Used to communicate with video
   *     capture device.
   * @param {!MediaStreamConstraints} constraints Constraints for preview
   *     stream.
   * @return {!Promise}
   * @abstract
   */
  prepareDevice(deviceOperator, constraints) {}

  /**
   * @return {!ModeBase}
   * @abstract
   */
  produce_() {}

  /**
   * Produces the mode capture object.
   * @return {!ModeBase}
   */
  produce() {
    const mode = this.produce_();
    this.clear();
    return mode;
  }

  /**
   * Clears all unused material.
   */
  clear() {
    this.stream_ = null;
    this.facing_ = Facing.UNKNOWN;
    this.captureResolution_ = null;
  }
}
