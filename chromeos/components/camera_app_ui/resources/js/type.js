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
    const [width, height] = s.split('x').map((x) => Number(x));
    return new Resolution(width, height);
  }

  /**
   * @override
   */
  toString() {
    return `${this.width}x${this.height}`;
  }
}

/**
 * Types of common mime types.
 * @enum {string}
 */
export const MimeType = {
  GIF: 'image/gif',
  JPEG: 'image/jpeg',
  MP4: 'video/mp4',
  PDF: 'application/pdf',
};

/**
 * Capture modes.
 * @enum {string}
 */
export const Mode = {
  PHOTO: 'photo',
  VIDEO: 'video',
  SQUARE: 'square',
  PORTRAIT: 'portrait',
  SCAN: 'scan',
};

/**
 * Camera facings.
 * @enum {string}
 */
export const Facing = {
  USER: 'user',
  ENVIRONMENT: 'environment',
  EXTERNAL: 'external',
  // VIRTUAL_{facing} is for labeling video device for configuring extra stream
  // from corresponding {facing} video device.
  VIRTUAL_USER: 'virtual_user',
  VIRTUAL_ENV: 'virtual_environment',
  VIRTUAL_EXT: 'virtual_external',
  NOT_SET: '(not set)',
};

/**
 * @enum {string}
 */
export const ViewName = {
  CAMERA: 'view-camera',
  DOCUMENT_MODE_DIALOG: 'view-document-mode-dialog',
  EXPERT_SETTINGS: 'view-expert-settings',
  FLASH: 'view-flash',
  GRID_SETTINGS: 'view-grid-settings',
  MESSAGE_DIALOG: 'view-message-dialog',
  PHOTO_RESOLUTION_SETTINGS: 'view-photo-resolution-settings',
  PTZ_PANEL: 'view-ptz-panel',
  RESOLUTION_SETTINGS: 'view-resolution-settings',
  REVIEW: 'view-review',
  SETTINGS: 'view-settings',
  SPLASH: 'view-splash',
  TIMER_SETTINGS: 'view-timer-settings',
  VIDEO_RESOLUTION_SETTINGS: 'view-video-resolution-settings',
  WARNING: 'view-warning',
};

/**
 * @enum {string}
 */
export const VideoType = {
  MP4: 'mp4',
  GIF: 'gif',
};


// The types here are used only in jsdoc and are required to be explicitly
// exported in order to be referenced by closure compiler.
// TODO(inker): Exports/Imports these jsdoc only types by closure compiler
// comment syntax. The implementation of syntax is tracked here:
// https://github.com/google/closure-compiler/issues/3041

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

/**
 * Type for performance event.
 * @enum {string}
 */
export const PerfEvent = {
  CAMERA_SWITCHING: 'camera-switching',
  GIF_CAPTURE_POST_PROCESSING: 'gif-capture-post-processing',
  LAUNCHING_FROM_LAUNCH_APP_COLD: 'launching-from-launch-app-cold',
  LAUNCHING_FROM_LAUNCH_APP_WARM: 'launching-from-launch-app-warm',
  LAUNCHING_FROM_WINDOW_CREATION: 'launching-from-window-creation',
  MODE_SWITCHING: 'mode-switching',
  PHOTO_CAPTURE_POST_PROCESSING: 'photo-capture-post-processing',
  PHOTO_CAPTURE_SHUTTER: 'photo-capture-shutter',
  PHOTO_TAKING: 'photo-taking',
  PORTRAIT_MODE_CAPTURE_POST_PROCESSING:
      'portrait-mode-capture-post-processing',
  VIDEO_CAPTURE_POST_PROCESSING: 'video-capture-post-processing',
};

/**
 * @typedef {{
 *   hasError: (boolean|undefined),
 *   resolution: (!Resolution|undefined),
 * }}
 */
export let PerfInformation;

/**
 * @typedef {{
 *   event: !PerfEvent,
 *   duration: number,
 *   perfInfo: (!PerfInformation|undefined),
 * }}
 */
export let PerfEntry;

/**
 * Error reported in testing run.
 * @typedef {{
 *   type: !ErrorType,
 *   level: !ErrorLevel,
 *   stack: string,
 *   time: number,
 *   name: string,
 * }}
 */
export let ErrorInfo;

/**
 * Types of error used in ERROR metrics.
 * @enum {string}
 */
export const ErrorType = {
  BROKEN_THUMBNAIL: 'broken-thumbnail',
  DEVICE_INFO_UPDATE_FAILURE: 'device-info-update-failure',
  DEVICE_NOT_EXIST: 'device-not-exist',
  EMPTY_FILE: 'empty-file',
  FILE_SYSTEM_FAILURE: 'file-system-failure',
  FRAME_ROTATION_NOT_DISABLED: 'frame-rotation-not-disabled',
  HANDLE_CAMERA_RESULT_FAILURE: 'handle-camera-result-failure',
  IDLE_DETECTOR_FAILURE: 'idle-detector-failure',
  INVALID_REVIEW_UI_STATE: 'invalid-review-ui-state',
  METADATA_MAPPING_FAILURE: 'metadata-mapping-failure',
  MULTIPLE_STREAMS_FAILURE: 'multiple-streams-failure',
  NO_AVAILABLE_LEVEL: 'no-available-level',
  PERF_METRICS_FAILURE: 'perf-metrics-failure',
  PRELOAD_IMAGE_FAILURE: 'preload-image-failure',
  SET_FPS_RANGE_FAILURE: 'set-fps-range-failure',
  START_CAMERA_FAILURE: 'start-camera-failure',
  START_CAPTURE_FAILURE: 'start-capture-failure',
  STOP_CAPTURE_FAILURE: 'stop-capture-failure',
  UNCAUGHT_PROMISE: 'uncaught-promise',
  UNKNOWN_FACING: 'unknown-facing',
  UNSAFE_INTEGER: 'unsafe-integer',
  UNSUPPORTED_PROTOCOL: 'unsupported-protocol',
};

/**
 * Error level used in ERROR metrics.
 * @enum {string}
 */
export const ErrorLevel = {
  WARNING: 'WARNING',
  ERROR: 'ERROR',
};

/**
 * Throws when a method is not implemented.
 */
export class NotImplementedError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'Method is not implemented') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an action is canceled.
 */
export class CanceledError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'The action is canceled') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an element fails to load a source.
 */
export class LoadError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'Source failed to load') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an media element fails to play.
 */
export class PlayError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'Media element failed to play') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an media element play a malformed file.
 */
export class PlayMalformedError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'Media element failed to play a malformed file') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the data to generate thumbnail is totally empty.
 */
export class EmptyThumbnailError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'The thumbnail is empty') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the recording is ended with no chunk returned.
 */
export class NoChunkError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(message = 'No chunk is received during recording session') {
    super(message);
    this.name = this.constructor.name;
  }
}
