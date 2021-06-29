// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {MimeType} from '../type.js';

/**
 * The singleton instance of MockDocumentScanner. Initialized by the first
 * invocation of getInstance().
 * @type {?MockDocumentScanner}
 */
let instance = null;

/**
 * Mock implementation for document scanner related APIs.
 */
export class MockDocumentScanner {
  /**
   * @public
   */
  constructor() {
    /**
     * @const {!Map<number, function(!Array<gfx.mojom.PointF>): void>}
     * @private
     */
    this.detectors_ = new Map();

    /**
     * @type {number}
     * @private
     */
    this.detectorIdCounter_ = 0;

    // Emulate the scanner which can provide scanning results in 10 fps.
    setInterval(() => {
      const corners = this.generateFakeCorners_();
      for (const detector of this.detectors_.values()) {
        detector(corners);
      }
    }, 100);
  }

  /**
   * Generates a set of fake detected corners.
   * @return {!Array<gfx.mojom.PointF>}
   * @private
   */
  generateFakeCorners_() {
    const makePoint = (x, y) => {
      return {x, y};
    };

    // Get a random offset in range [-0.05, 0.05).
    const randomOffset = () => {
      return Math.random() * 0.1 - 0.05;
    };

    const corners = [
      makePoint(0.1 + randomOffset(), 0.1 + randomOffset()),
      makePoint(0.1 + randomOffset(), 0.9 + randomOffset()),
      makePoint(0.9 + randomOffset(), 0.9 + randomOffset()),
      makePoint(0.9 + randomOffset(), 0.1 + randomOffset()),
    ];
    return corners;
  }

  /**
   * Returns true if document mode is supported on the device.
   * @return {!Promise<{isSupported: boolean}>}
   */
  async isDocumentModeSupported() {
    return {isSupported: true};
  }

  /**
   * Registers the document corners detector for preview and returns the
   * detector |id|.
   * @param {function(!Array<gfx.mojom.PointF>): void} callback Callback to
   *     trigger when the detected corners are updated.
   * @return {!Promise<{id: number}>}
   */
  async registerDocumentCornersDetector(callback) {
    this.detectorIdCounter_++;
    this.detectors_.set(this.detectorIdCounter_, callback);
    return {id: this.detectorIdCounter_};
  }

  /**
   * Unregister the document corners detector by given |id|.
   * @param {number} detectorId
   * @return {!Promise<{isSuccess: boolean}>}
   */
  async unregisterDocumentCornersDetector(detectorId) {
    return {isSuccess: this.detectors_.delete(detectorId)};
  }

  /**
   * Returns the detected document corners from given |jpeg_data|. The
   * coordinate space of |corners| will be in [0, 1].
   * @param {!Array<number>} jpegData
   * @return {!Promise<{corners: !Array<!gfx.mojom.PointF>}>}
   */
  async scanDocumentCorners(jpegData) {
    return {corners: this.generateFakeCorners_()};
  }

  /**
   * Does the post processing for document given by its |jpeg_data|, document
   * |corners|, and convert the result to the desired |output_format|.
   * @param {!Array<number>} jpegData
   * @param {!Array<!gfx.mojom.PointF>} corners
   * @param {!MimeType} outputFormat
   * @return {!Promise<{processedData: !Array<number>}>}
   */
  async convertToDocument(jpegData, corners, outputFormat) {
    return {processedData: jpegData};
  }

  /**
   * Creates a new instance of MockDocumentScanner if it is not set. Returns
   * the exist instance.
   * @return {!MockDocumentScanner} The singleton instance.
   */
  static getInstance() {
    if (instance === null) {
      instance = new MockDocumentScanner();
    }
    return instance;
  }
}
