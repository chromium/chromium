// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../../dom.js';
import {I18nString} from '../../../i18n_string.js';
import {speak} from '../../../toast.js';

/**
 * Controller for the record-time of Camera view.
 */
export class RecordTime {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!HTMLElement}
     * @private
     */
    this.recordTime_ = dom.get('#record-time', HTMLElement);

    /**
     * Timeout to count every tick of elapsed recording time.
     * @type {?number}
     * @private
     */
    this.tickTimeout_ = null;

    /**
     * Tick count of elapsed recording time.
     * @type {number}
     * @private
     */
    this.ticks_ = 0;

    /**
     * The timestamp when the recording starts.
     * @type {number}
     * @private
     */
    this.startTimestamp_ = 0;

    /**
     * The total duration of the recording in milliseconds.
     * @type {number}
     * @private
     */
    this.totalDuration_ = 0;
  }

  /**
   * Updates UI by the elapsed recording time.
   * @param {number} time Time in seconds.
   * @private
   */
  update_(time) {
    // Format time into HH:MM:SS or MM:SS.
    const pad = (n) => {
      return (n < 10 ? '0' : '') + n;
    };
    let hh = '';
    if (time >= 3600) {
      hh = pad(Math.floor(time / 3600)) + ':';
    }
    const mm = pad(Math.floor(time / 60) % 60) + ':';
    dom.get('#record-time-msg', HTMLElement).textContent =
        hh + mm + pad(time % 60);
  }

  /**
   * Starts to count and show the elapsed recording time.
   * @param {{resume: boolean}} params If the time count is resumed from paused
   *     state.
   */
  start({resume}) {
    if (!resume) {
      this.ticks_ = 0;
      this.totalDuration_ = 0;
    }
    this.update_(this.ticks_);
    this.recordTime_.hidden = false;

    this.tickTimeout_ = setInterval(() => {
      this.ticks_++;
      this.update_(this.ticks_);
    }, 1000);

    this.startTimestamp_ = performance.now();
  }

  /**
   * Stops counting and showing the elapsed recording time.
   * @param {{pause: boolean}} param If the time count is paused temporarily.
   */
  stop({pause}) {
    speak(I18nString.STATUS_MSG_RECORDING_STOPPED);
    if (this.tickTimeout_) {
      clearInterval(this.tickTimeout_);
      this.tickTimeout_ = null;
    }
    if (!pause) {
      this.ticks_ = 0;
      this.recordTime_.hidden = true;
      this.update_(0);
    }

    this.totalDuration_ += performance.now() - this.startTimestamp_;
  }

  /**
   * Returns the recorded duration in minutes.
   * @return {number}
   */
  inMinutes() {
    return Math.ceil(this.totalDuration_ / 1000 / 60);
  }

  /**
   * Returns the recorded duration in milliseconds.
   * @return {number}
   */
  inMilliseconds() {
    return this.totalDuration_;
  }
}
