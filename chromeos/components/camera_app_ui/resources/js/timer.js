// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './chrome_util.js';

/**
 * A one-shot timer that is more powerful than setTimeout().
 */
export class OneShotTimer {
  /**
   * The parameters are same as the parameters of setTimeout().
   * @param {function()} handler
   * @param {number} timeout
   */
  constructor(handler, timeout) {
    /**
     * @type {function()}
     * @const
     * @private
     */
    this.handler_ = handler;

    /**
     * @type {number}
     * @const
     * @private
     */
    this.timeout_ = timeout;

    /**
     * @type {number}
     * @private
     */
    this.timeoutId_ = 0;

    this.start_();
  }

  /**
   * Resets the timer delay.
   */
  resetTimeout() {
    this.clearPendingTimeout_();
    this.start_();
  }

  /**
   * Stops the timer and runs the scheduled handler immediately.
   */
  fireNow() {
    this.clearPendingTimeout_();
    this.handler_();
  }

  /**
   * Starts the timer.
   * @private
   */
  start_() {
    assert(this.timeoutId_ === 0);
    this.timeoutId_ = setTimeout(this.handler_, this.timeout_);
  }

  /**
   * Clears the pending timeout.
   * @private
   */
  clearPendingTimeout_() {
    assert(this.timeoutId_ !== 0);
    clearTimeout(this.timeoutId_);
    this.timeoutId_ = 0;
  }
}
