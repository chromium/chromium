// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Photo or video resolution.
 */
export class Resolution {
  /**
   * @param {number} width
   * @param {number} height
   */
  constructor(width, height) {
    /**
     * @type {number}
     * @const
     */
    this.width = width;

    /**
     * @type {number}
     * @const
     */
    this.height = height;
  }

  /**
   * @return {number} Total pixel number.
   */
  get area() {
    return this.width * this.height;
  }

  /**
   * Aspect ratio calculates from width divided by height.
   * @return {number}
   */
  get aspectRatio() {
    // Approxitate to 4 decimal places to prevent precision error during
    // comparing.
    return parseFloat((this.width / this.height).toFixed(4));
  }

  /**
   * Compares width/height of resolutions, see if they are equal or not.
   * @param {!Resolution} resolution Resolution to be compared with.
   * @return {boolean} Whether width/height of resolutions are equal.
   */
  equals(resolution) {
    return this.width === resolution.width && this.height === resolution.height;
  }

  /**
   * Compares aspect ratio of resolutions, see if they are equal or not.
   * @param {!Resolution} resolution Resolution to be compared with.
   * @return {boolean} Whether aspect ratio of resolutions are equal.
   */
  aspectRatioEquals(resolution) {
    return this.width * resolution.height === this.height * resolution.width;
  }

  /**
   * Create Resolution object from string.
   * @param {string} s
   * @return {!Resolution}
   */
  static fromString(s) {
    return new Resolution(...s.split('x').map(Number));
  }

  /**
   * @override
   */
  toString() {
    return `${this.width}x${this.height}`;
  }
}

/**
 * Capture modes.
 * @enum {string}
 */
export const Mode = {
  PHOTO: 'photo',
  VIDEO: 'video',
  SQUARE: 'square',
  PORTRAIT: 'portrait',
};

/**
 * Camera facings.
 * @enum {string}
 */
export const Facing = {
  USER: 'user',
  ENVIRONMENT: 'environment',
  EXTERNAL: 'external',
  NOT_SET: '(not set)',
  UNKNOWN: 'unknown',
};

/**
 * @enum {string}
 */
export const ViewName = {
  CAMERA: 'view-camera',
  EXPERT_SETTINGS: 'view-expert-settings',
  GRID_SETTINGS: 'view-grid-settings',
  MESSAGE_DIALOG: 'view-message-dialog',
  PHOTO_RESOLUTION_SETTINGS: 'view-photo-resolution-settings',
  RESOLUTION_SETTINGS: 'view-resolution-settings',
  SETTINGS: 'view-settings',
  SPLASH: 'view-splash',
  TIMER_SETTINGS: 'view-timer-settings',
  VIDEO_RESOLUTION_SETTINGS: 'view-video-resolution-settings',
  WARNING: 'view-warning',
};

// The types here are used only in jsdoc and are required to be explicitly
// exported in order to be referenced by closure compiler.
// TODO(inker): Exports/Imports these jsdoc only types by closure compiler
// comment syntax. The implementation of syntax is tracked here:
// https://github.com/google/closure-compiler/issues/3041

/**
 * @typedef {{
 *   hasError: (boolean|undefined),
 *   resolution: (!Resolution|undefined),
 * }}
 */
export let PerfInformation;

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 *   maxFps: number,
 * }}
 */
export let VideoConfig;

/**
 * @typedef {{
 *   minFps: number,
 *   maxFps: number,
 * }}
 */
export let FpsRange;

/**
 * A list of resolutions.
 * @typedef {!Array<!Resolution>}
 */
export let ResolutionList;

/**
 * Map of all available resolution to its maximal supported capture fps. The key
 * of the map is the resolution and the corresponding value is the maximal
 * capture fps under that resolution.
 * @typedef {!Object<(!Resolution|string), number>}
 */
export let MaxFpsInfo;

/**
 * List of supported capture fps ranges.
 * @typedef {!Array<!FpsRange>}
 */
export let FpsRangeList;
