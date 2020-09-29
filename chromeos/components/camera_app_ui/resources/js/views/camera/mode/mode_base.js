// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
