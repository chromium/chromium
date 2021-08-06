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

// TODO(b/172881094): Upstream the externs of PTZ fields to Closure Compiler.

/** @type {!MediaSettingsRange} */
MediaTrackCapabilities.prototype.pan;

/** @type {!MediaSettingsRange} */
MediaTrackCapabilities.prototype.tilt;

/** @type {number} */
MediaTrackSettings.prototype.pan;

/** @type {number} */
MediaTrackSettings.prototype.tilt;

// TODO(b/172881094): Upstream the externs of pointer event to Closure Compiler.
// https://www.w3.org/TR/pointerevents2/#dom-globaleventhandlers-onpointerdown

/** @type {?function (Event)} */ Element.prototype.onpointerdown;
/** @type {?function (Event)} */ Element.prototype.onpointerup;
/** @type {?function (Event)} */ Element.prototype.onpointerleave;

/**
 * @record
 * @struct
 */
function OverconstrainedError() {}

/** @type {string} */
OverconstrainedError.prototype.constraint;

/** @type {string} */
OverconstrainedError.prototype.name;

/** @type {string} */
OverconstrainedError.prototype.message;


// CSS Typed OM Level 1: https://drafts.css-houdini.org/css-typed-om/

/**
 * @constructor
 */
function StylePropertyMapReadOnly() {}

/**
 * @param {string} property
 * @return {?CSSStyleValue}
 */
StylePropertyMapReadOnly.prototype.get = function(property) {};

/**
 * @param {string} property
 * @return {boolean}
 */
StylePropertyMapReadOnly.prototype.has = function(property) {};

/**
 * @return {!StylePropertyMapReadOnly}
 */
Element.prototype.computedStyleMap = function() {};

// The base StylePropertyMap is defined in
// third_party/closure_compiler/externs/pending.js, but missing extend for
// StylePropertyMapReadOnly.

/**
 * @param {string} property
 * @return {?CSSStyleValue}
 */
StylePropertyMap.prototype.get = function(property) {};

/**
 * @param {string} property
 * @return {boolean}
 */
StylePropertyMap.prototype.has = function(property) {};

/**
 * @constructor
 */
function CSSStyleValue() {}

/**
 * @constructor
 * @extends {CSSStyleValue}
 */
function CSSNumericValue() {}

/**
 * @param {string} cssText
 * @return {!CSSNumericValue}
 */
CSSNumericValue.parse = function(cssText) {};

/**
 * @param {string} unit
 * @return {!CSSUnitValue}
 */
CSSNumericValue.prototype.to = function(unit) {};

/**
 * @typedef {number|CSSNumericValue}
 */
let CSSNumberish;

/**
 * @typedef {Object}
 */
let CSSTransformComponent;

/**
 * @constructor
 * @extends {CSSTransformComponent}
 * @param {!CSSNumericValue} x
 * @param {!CSSNumericValue} y
 */
function CSSTranslate(x, y) {}

/**
 * @constructor
 * @extends {CSSTransformComponent}
 * @param {!CSSNumericValue} angle
 */
function CSSRotate(angle) {}

/**
 * @type {!CSSNumericValue}
 */
CSSRotate.prototype.angle;

/**
 * @constructor
 * @extends {CSSTransformComponent}
 * @param {!CSSNumberish} x
 * @param {!CSSNumberish} y
 */
function CSSScale(x, y) {}

/**
 * @constructor
 * @extends {CSSStyleValue}
 * @implements {Iterable<!CSSTransformComponent>}
 * @param {!Array<!CSSTransformComponent>} transforms
 */
function CSSTransformValue(transforms) {}

/**
 * @constructor
 * @extends {CSSNumericValue}
 * @param {number} value
 * @param {string} unit
 */
function CSSUnitValue(value, unit) {}

/**
 * @type {number}
 */
CSSUnitValue.prototype.value;

/**
 * @param {number} px
 * @return {!CSSUnitValue}
 */
CSS.px = function(px) {};

/**
 * @param {number} rad
 * @return {!CSSUnitValue}
 */
CSS.rad = function(rad) {};

/**
 * @param {number} number
 * @return {!CSSUnitValue}
 */
CSS.number = function(number) {};

// CSS Properties and Values API Level 1
// https://www.w3.org/TR/css-properties-values-api-1/
/**
 * @typedef {{
 *   name: string,
 *   syntax: ?string,
 *   inherits: boolean,
 *   initialValue: ?string,
 * }}
 */
var PropertyDefinition;

/**
 * @param {PropertyDefinition} definition
 */
CSS.registerProperty = function(definition) {};
