// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(calamity): Remove TestTimerProxy in favor of MockTimer.
export class TestTimerProxy {
  constructor() {
    this.immediatelyResolveTimeouts = true;

    /** @private {number} */
    this.nextTimeoutId_ = 0;

    /** @private {!Map<number, !Function>} */
    this.activeTimeouts_ = new Map();
  }

  /**
   * @param {Function} fn
   * @param {number=} delay
   * @return {number}
   * @override
   */
  setTimeout(fn, delay) {
    if (this.immediatelyResolveTimeouts) {
      fn();
    } else {
      this.activeTimeouts_.set(this.nextTimeoutId_, fn);
    }

    return this.nextTimeoutId_++;
  }

  /**
   * @param {number} id
   * @override
   */
  clearTimeout(id) {
    this.activeTimeouts_.delete(id);
  }

  /**
   * Run the function associated with a timeout id and clear it from the
   * active timeouts.
   * @param {number} id
   */
  runTimeoutFn(id) {
    this.activeTimeouts_.get(id)();
    this.clearTimeout(id);
  }

  /**
   * Returns true if a given timeout id has not been run or cleared.
   * @param {number} id
   * @return {boolean} Whether a given timeout id has not been run or
   * cleared.
   */
  hasTimeout(id) {
    return this.activeTimeouts_.has(id);
  }
}
