// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from '../lib/comlink.js';

// eslint-disable-next-line no-unused-vars
import {BarcodeWorkerInterface} from './barcode_worker_interface.js';

/**
 * A barcode worker to detect barcode from images.
 * @implements {BarcodeWorkerInterface}
 */
class BarcodeWorker {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!BarcodeDetector}
     * @private
     */
    this.detector_ = new BarcodeDetector({formats: ['qr_code']});
  }

  /**
   * @override
   */
  async detect(bitmap) {
    const codes = await this.detector_.detect(bitmap);

    if (codes.length === 0) {
      return null;
    }

    const cx = bitmap.width / 2;
    const cy = bitmap.height / 2;
    /**
     * @param {!DetectedBarcode} code
     * @return {number}
     */
    const distanceToCenter = (code) => {
      const {left, right, top, bottom} = code.boundingBox;
      const x = (left + right) / 2;
      const y = (top + bottom) / 2;
      return Math.hypot(x - cx, y - cy);
    };

    let bestCode = codes[0];
    let minDistance = distanceToCenter(codes[0]);
    for (let i = 1; i < codes.length; i++) {
      const distance = distanceToCenter(codes[i]);
      if (distance < minDistance) {
        bestCode = codes[i];
        minDistance = distance;
      }
    }
    return bestCode.rawValue;
  }
}

Comlink.expose(new BarcodeWorker());
