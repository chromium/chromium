// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../../dom.js';
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
    document.querySelector('#record-time-msg').textContent =
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
    }
    this.update_(this.ticks_);
    this.recordTime_.hidden = false;

    this.tickTimeout_ = setInterval(() => {
      this.ticks_++;
      this.update_(this.ticks_);
    }, 1000);
  }

  /**
   * Stops counting and showing the elapsed recording time.
   * @param {{pause: boolean}} param If the time count is paused temporarily.
   * @return {number} Recorded time in 1 minute buckets.
   */
  stop({pause}) {
    speak('status_msg_recording_stopped');
    if (this.tickTimeout_) {
      clearInterval(this.tickTimeout_);
      this.tickTimeout_ = null;
    }
    const mins = Math.ceil(this.ticks_ / 60);
    if (!pause) {
      this.ticks_ = 0;
      this.recordTime_.hidden = true;
      this.update_(0);
    }
    return mins;
  }
}
