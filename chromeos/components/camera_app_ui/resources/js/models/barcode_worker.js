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

    // TODO(b/172879638): Handle multiple barcodes.
    return codes[0].rawValue;
  }
}

Comlink.expose(new BarcodeWorker());
