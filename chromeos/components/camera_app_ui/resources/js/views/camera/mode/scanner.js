// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Facing,      // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

import {
  Photo,
  PhotoBaseFactory,
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class ScannerHandler {}

/**
 * Photo mode capture controller.
 */
export class Scanner extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution
   * @param {!ScannerHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, /** @type {!PhotoHandler} */ ({}));

    /**
     * @const {!ScannerHandler}
     * @protected
     */
    this.scannerHandler_ = handler;
  }
}

/**
 * Factory for creating photo mode capture object.
 */
export class ScannerFactory extends PhotoBaseFactory {
  /**
   * @param {!ScannerHandler} handler
   */
  constructor(handler) {
    super();

    /**
     * @const {!ScannerHandler}
     * @protected
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  produce_() {
    return new Scanner(
        this.previewStream_,
        this.facing_,
        this.captureResolution_,
        this.handler_,
    );
  }
}
