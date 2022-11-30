// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used by |FakeBarcodeDetector|, if false |FakeBarcodeDetector|
 * will return truthy values.
 * @type {bool}
 */
let shouldBarcodeDetectionFail = false;

/**
 * The barcode returned by successful calls to detect().
 * @type {string}
 */
let detectedBarcode = 'LPA:1$ACTIVATION_CODE';

/**
 * @implements {BarcodeDetector}
 */
export class FakeBarcodeDetector {
  constructor() {}

  /** @override */
  detect() {
    if (shouldBarcodeDetectionFail) {
      return Promise.reject('Failed to detect code');
    }
    return Promise.resolve([{rawValue: detectedBarcode}]);
  }

  /** @override */
  static getSupportedFormats() {
    if (shouldBarcodeDetectionFail) {
      return Promise.resolve([]);
    }

    return Promise.resolve(['qr_code', 'code_39', 'codabar']);
  }

  static setShouldFail(shouldFail) {
    shouldBarcodeDetectionFail = shouldFail;
  }

  /**
   * @param {string} barcode
   */
  static setDetectedBarcode(barcode) {
    detectedBarcode = barcode;
  }
}

/**
 * @implements {ImageCapture}
 */
export class FakeImageCapture {
  constructor(mediaStream) {
    this.track = {
      readyState: 'live',
      enabled: true,
      muted: false,
    };
  }

  /** @override */
  grabFrame() {
    return null;
  }
}
