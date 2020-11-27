// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
     * @type {!BarcodeDetector}
     * @private
     */
    this.detector_ = new BarcodeDetector({formats: ['qr_code']});

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
    // TODO(b/172879638): Add a setIntervalAsync() helper to avoid two
    // detections running at the same time.
    this.intervalId_ = setInterval(async () => {
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
    clearInterval(this.intervalId_);
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
    // TODO(b/172879638): Run this on a Web Worker.
    const codes = await this.detector_.detect(this.video_);

    if (codes.length === 0) {
      return null;
    }
    // TODO(b/172879638): Handle multiple barcodes.
    return codes[0].rawValue;
  }
}
