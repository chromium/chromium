// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used by |FakeBarcodeDetector|, if false |FakeBarcodeDetector|
 * will return truthy values.
 */
let shouldBarcodeDetectionFail: boolean = false;

/**
 * The barcode returned by successful calls to detect().
 */
let detectedBarcode: string = 'LPA:1$ACTIVATION_CODE';

export class FakeBarcodeDetector implements BarcodeDetector {
  constructor(_: BarcodeDetectorOptions) {}

  detect(_: ImageBitmapSource): Promise<DetectedBarcode[]> {
    if (shouldBarcodeDetectionFail) {
      return Promise.reject('Failed to detect code');
    }
    // Esim flow only uses rawValue.
    return Promise.resolve([{
      boundingBox: new DOMRectReadOnly(0, 0, 1, 1),
      format: 'qr_code',
      cornerPoints: [{x: 0, y: 0}, {x: 1, y: 0}, {x: 1, y: 1}, {x: 0, y: 1}],
      rawValue: detectedBarcode,
    }]);
  }

  static getSupportedFormats(): Promise<BarcodeFormat[]> {
    if (shouldBarcodeDetectionFail) {
      return Promise.resolve([]);
    }

    return Promise.resolve(['qr_code', 'code_39', 'codabar']);
  }

  static setShouldFail(shouldFail: boolean): void {
    shouldBarcodeDetectionFail = shouldFail;
  }

  static setDetectedBarcode(barcode: string): void {
    detectedBarcode = barcode;
  }
}

export class FakeImageCapture implements ImageCapture {
  track: MediaStreamTrack;

  constructor(track: MediaStreamTrack) {
    this.track = track;
  }

  takePhoto(_: PhotoSettings): Promise<Blob> {
    // Return a fake Blob because esim flow only used grabFrame().
    return Promise.resolve(new Blob(['FakeImageData'], {type: 'image/jpeg'}));
  }

  getPhotoCapabilities(): Promise<PhotoCapabilities> {
    // Return a fake PhotoCapabilities because esim flow only used grabFrame().
    return Promise.resolve({
      fillLightMode: ['auto', 'flash'],
      redEyeReduction: 'never',
      imageHeight: {
        min: 0,
        max: 480,
        step: 10,
      },
      imageWidth: {
        min: 0,
        max: 640,
        step: 10,
      },
    });
  }

  getPhotoSettings(): Promise<PhotoSettings> {
    // Return a fake PhotoSettings because esim flow only used grabFrame().
    return Promise.resolve({});
  }

  grabFrame(): Promise<ImageBitmap> {
    return Promise.resolve({
      width: 0,
      height: 0,
      close: () => {},
    });
  }
}
