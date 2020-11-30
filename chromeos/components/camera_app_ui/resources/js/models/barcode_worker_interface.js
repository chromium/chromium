// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The interface for a barcode worker. All methods are marked as async since
 * it will be used with Comlink and Web Workers.
 * @interface
 */
export class BarcodeWorkerInterface {
  /**
   * Detects barcodes from an image bitmap.
   * @param {!ImageBitmap} bitmap
   * @return {!Promise<?string>} The detected barcode value, or null if no
   *     barcode is detected.
   * @abstract
   */
  async detect(bitmap) {}
}
