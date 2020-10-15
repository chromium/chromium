// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeHelper} from './mojo/chrome_helper.js';
// eslint-disable-next-line no-unused-vars
import {PerfEntry, PerfEvent, PerfInformation} from './type.js';

/**
 * @typedef {function(!PerfEntry): void}
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
   * @param {number=} startTime The start time of the event.
   */
  start(event, startTime = performance.now()) {
    if (this.startTimeMap_.has(event)) {
      console.error(`Failed to start event ${event} since the previous one is
                     not stopped.`);
      return;
    }
    this.startTimeMap_.set(event, startTime);
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
    this.listeners_.forEach(
        (listener) => listener({event, duration, perfInfo}));
  }

  /**
   * Records the time of the interruption.
   */
  interrupt() {
    this.interruptedTime_ = performance.now();
  }
}
