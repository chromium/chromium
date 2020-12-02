// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-undef, no-unused-vars, no-var, valid-jsdoc */

// TODO(b/172879638): Remove this extern once we have
// https://github.com/google/closure-compiler/pull/3735 merged in Closure
// Compiler and Chromium.

/** @type {string} */
OffscreenCanvasRenderingContext2D.prototype.imageSmoothingQuality;

// TODO(b/172879638): Upstream the externs of BarcodeDetector to Closure
// Compiler.

/**
 * @typedef {HTMLImageElement|HTMLVideoElement|HTMLCanvasElement|ImageBitmap|
 *     OffscreenCanvas}
 */
var CanvasImageSource;

/**
 * @typedef {!CanvasImageSource|!Blob|!ImageData}
 * @see https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#imagebitmapsource
 */
var ImageBitmapSource;

/**
 * @record
 * @struct
 */
function BarcodeDetectorOptions() {}

/** @type {!Array<string>} */
BarcodeDetectorOptions.prototype.formats;

/**
 * @record
 * @struct
 */
function DetectedBarcode() {}

/** @type {!DOMRectReadOnly} */
DetectedBarcode.prototype.boundingBox;

/** @type {!Array<{x: number, y: number}>} */
DetectedBarcode.prototype.cornerPoints;

/** @type {string} */
DetectedBarcode.prototype.format;

/** @type {string} */
DetectedBarcode.prototype.rawValue;

/**
 * @constructor
 * @param {!BarcodeDetectorOptions=} barcodeDetectorOptions
 * @see https://wicg.github.io/shape-detection-api/#barcode-detection-api
 */
function BarcodeDetector(barcodeDetectorOptions) {}

/**
 * @return {!Promise<!Array<string>>}
 */
BarcodeDetector.getSupportedFormats = function() {};

/**
 * @param {!ImageBitmapSource} image
 * @return {!Promise<!Array<!DetectedBarcode>>}
 */
BarcodeDetector.prototype.detect = function(image) {};
