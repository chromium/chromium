// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from '../lib/comlink.js';

import {clearAsyncInterval, setAsyncInterval} from './async_interval.js';
// eslint-disable-next-line no-unused-vars
import {BarcodeWorkerInterface} from './barcode_worker_interface.js';

// TODO(b/172879638): Get some performance data and tune the scan interval.
const SCAN_INTERVAL = 1000;

/**
 * A barcode scanner to detect barcodes from a camera stream.
 */
export class BarcodeScanner {
  /**
   * @param {!HTMLVideoElement} video The video to be scanned for barcode.
   * @param {function(string)} callback The callback for the detected barcodes.
   */
  constructor(video, callback) {
    /**
     * @type {!HTMLVideoElement}
     * @private
     */
    this.video_ = video;

    /**
     * @type {function(string)}
     * @private
     */
    this.callback_ = callback;

    /**
     * @type {!BarcodeWorkerInterface}
     * @private
     */
    this.worker_ = Comlink.wrap(
        new Worker('/js/models/barcode_worker.js', {type: 'module'}));

    /**
     * The current running interval id.
     * @type {?number}
     */
    this.intervalId_ = null;
  }

  /**
   * Starts scanning barcodes continuously. Calling this method when it's
   * already started would be no-op.
   */
  start() {
    if (this.intervalId_ !== null) {
      return;
    }
    let prevCode = null;
    this.intervalId_ = setAsyncInterval(async () => {
      const code = await this.scan_();
      if (code !== null && code !== prevCode) {
        prevCode = code;
        this.callback_(code);
      }
    }, SCAN_INTERVAL);
  }

  /**
   * Stops scanning barcodes.
   */
  stop() {
    if (this.intervalId_ === null) {
      return;
    }
    clearAsyncInterval(this.intervalId_);
    this.intervalId_ = null;
  }

  /**
   * Scans barcodes from the current frame.
   * @return {!Promise<?string>} The detected barcode value, or null if no
   *     barcode is detected.
   * @private
   */
  async scan_() {
    // TODO(b/172879638): Down scale the frame if the resolution is too high.
    const bitmap = await createImageBitmap(this.video_);

    const value = await this.worker_.detect(Comlink.transfer(bitmap, [bitmap]));
    return value;
  }
}
