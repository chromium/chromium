// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used by |FakeBarcodeDetector|, if false |FakeBarcodeDetector|
 * will return truthy values.
 * @type {bool}
 */
let shouldBarcodeDetectionFail = false;

/**
 * @implements {BarcodeDetector}
 */
/* #export */ class FakeBarcodeDetector {
  constructor() {}

  /** @override */
  detect() {
    if (shouldBarcodeDetectionFail) {
      return Promise.reject('Failed to detect code');
    }
    return Promise.resolve([{rawValue: 'testbarcode'}]);
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
}

/**
 * @implements {ImageCapture}
 */
/* #export */ class FakeImageCapture {
  constructor(mediaStream) {}

  /** @override */
  grabFrame() {
    return null;
  }
}