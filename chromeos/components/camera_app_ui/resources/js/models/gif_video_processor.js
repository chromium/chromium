// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {AsyncWriter} from './async_writer.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessor} from './video_processor_interface.js';

/**
 * A video processor that creates gif from frames.
 * @implements {VideoProcessor}
 */
export class GIFVideoProcessor {
  /**
   * @param {!AsyncWriter} output The output writer of gif.
   * @param {number} width
   * @param {number} height
   */
  constructor(output, width, height) {
    /**
     * @const {!AsyncWriter}
     * @private
     */
    this.output_ = output;
  }

  /**
   * @override
   */
  async write(frame) {
    // TODO(b:191950622): Add frame encoding mechanism.
  }

  /**
   * @override
   */
  async close() {
    // TODO(b:191950622): Add finish logistics.
    this.output_.close();
  }

  /**
   * @override
   */
  async cancel() {}
}
