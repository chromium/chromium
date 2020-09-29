// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeHelper} from './mojo/chrome_helper.js';
// eslint-disable-next-line no-unused-vars
import {PerfInformation} from './type.js';

/**
 * Type for performance event.
 * @enum {string}
 */
export const PerfEvent = {
  PHOTO_TAKING: 'photo-taking',
  PHOTO_CAPTURE_SHUTTER: 'photo-capture-shutter',
  PHOTO_CAPTURE_POST_PROCESSING: 'photo-capture-post-processing',
  VIDEO_CAPTURE_POST_PROCESSING: 'video-capture-post-processing',
  PORTRAIT_MODE_CAPTURE_POST_PROCESSING:
      'portrait-mode-capture-post-processing',
  MODE_SWITCHING: 'mode-switching',
  CAMERA_SWITCHING: 'camera-switching',
  LAUNCHING_FROM_WINDOW_CREATION: 'launching-from-window-creation',
  LAUNCHING_FROM_LAUNCH_APP_COLD: 'launching-from-launch-app-cold',
  LAUNCHING_FROM_LAUNCH_APP_WARM: 'launching-from-launch-app-warm',
};

/**
 * @typedef {function(!PerfEvent, number, !Object=)}
 */
let PerfEventListener;  // eslint-disable-line no-unused-vars

/**
 * Logger for performance events.
 */
export class PerfLogger {
  /**
   * @public
   */
  constructor() {
    /**
     * Map to store events starting timestamp.
     * @type {!Map<!PerfEvent, number>}
     * @private
     */
    this.startTimeMap_ = new Map();

    /**
     * Set of the listeners for perf events.
     * @type {!Set<!PerfEventListener>}
     */
    this.listeners_ = new Set();

    /**
     * The timestamp when the measurement is interrupted.
     * @type {?number}
     */
    this.interruptedTime_ = null;
  }

  /**
   * Adds listener for perf events.
   * @param {!PerfEventListener} listener
   */
  addListener(listener) {
    this.listeners_.add(listener);
  }

  /**
   * Removes listener for perf events.
   * @param {!PerfEventListener} listener
   * @return {boolean} Returns true if remove successfully. False otherwise.
   */
  removeListener(listener) {
    return this.listeners_.delete(listener);
  }

  /**
   * Starts the measurement for given event.
   * @param {!PerfEvent} event Target event.
   */
  start(event) {
    if (this.startTimeMap_.has(event)) {
      console.error(`Failed to start event ${event} since the previous one is
                     not stopped.`);
      return;
    }
    this.startTimeMap_.set(event, performance.now());
    ChromeHelper.getInstance().startTracing(event);
  }

  /**
   * Stops the measurement for given event and returns the measurement result.
   * @param {!PerfEvent} event Target event.
   * @param {!PerfInformation=} perfInfo Optional information of this event
   *     for performance measurement.
   */
  stop(event, perfInfo = {}) {
    if (!this.startTimeMap_.has(event)) {
      console.error(`Failed to stop event ${event} which is never started.`);
      return;
    }

    const startTime = this.startTimeMap_.get(event);
    this.startTimeMap_.delete(event);

    // If there is error during performance measurement, drop it since it might
    // be inaccurate.
    if (perfInfo.hasError) {
      return;
    }

    // If the measurement is interrupted, drop the measurement since the result
    // might be inaccurate.
    if (this.interruptedTime_ !== null && startTime < this.interruptedTime_) {
      return;
    }

    const duration = performance.now() - startTime;
    ChromeHelper.getInstance().stopTracing(event);
    this.listeners_.forEach((listener) => listener(event, duration, perfInfo));
  }

  /**
   * Stops the measurement of launch-related events.
   * @param {!PerfInformation=} perfInfo Optional information of this event
   *     for performance measurement.
   */
  stopLaunch(perfInfo) {
    const launchEvents = [
      PerfEvent.LAUNCHING_FROM_WINDOW_CREATION,
      PerfEvent.LAUNCHING_FROM_LAUNCH_APP_COLD,
      PerfEvent.LAUNCHING_FROM_LAUNCH_APP_WARM,
    ];

    launchEvents.forEach((event) => {
      if (this.startTimeMap_.has(event)) {
        this.stop(event, perfInfo);
      }
    });
  }

  /**
   * Records the time of the interruption.
   */
  interrupt() {
    this.interruptedTime_ = performance.now();
  }
}
