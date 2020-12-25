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

    this.start();
  }

  /**
   * Starts the timer.
   */
  start() {
    assert(this.timeoutId_ === 0);
    this.timeoutId_ = setTimeout(this.handler_, this.timeout_);
  }

  /**
   * Stops the pending timeout.
   */
  stop() {
    assert(this.timeoutId_ !== 0);
    clearTimeout(this.timeoutId_);
    this.timeoutId_ = 0;
  }

  /**
   * Resets the timer delay. It's a no-op if the timer is already stopped.
   */
  resetTimeout() {
    if (this.timeoutId_ === 0) {
      return;
    }
    this.stop();
    this.start();
  }

  /**
   * Stops the timer and runs the scheduled handler immediately.
   */
  fireNow() {
    if (this.timeoutId_ !== 0) {
      this.stop();
    }
    this.handler_();
  }
}
