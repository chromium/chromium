// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Overrides timeout and interval callbacks to mock timing behavior. */
/* #export */ class MockTimer {
  constructor() {
    /**
     * Default versions of the timing functions.
     * @type {Object<string, !Function>}
     * @private
     */
    this.originals_ = [];

    /**
     * Key to assign on the next creation of a scheduled timer. Each call to
     * setTimeout or setInterval returns a unique key that can be used for
     * clearing the timer.
     * @type {number}
     * @private
     */
    this.nextTimerKey_ = 1;

    /**
     * Details for active timers.
     * @type {Array<{callback: Function,
     *                delay: number,
     *                key: number,
     *                repeats: boolean}>}
     * @private
     */
    this.timers_ = [];

    /**
     * List of scheduled tasks.
     * @type {Array<{when: number, key: number}>}
     * @private
     */
    this.schedule_ = [];

    /**
     * Virtual elapsed time in milliseconds.
     * @type {number}
     * @private
     */
    this.now_ = 0;

    /**
     * Used to control when scheduled callbacks fire.  Calling the 'tick' method
     * inflates this parameter and triggers callbacks.
     * @type {number}
     * @private
     */
    this.until_ = 0;
  }

  /**
   * Replaces built-in functions for scheduled callbacks.
   */
  install() {
    this.replace_('setTimeout', this.setTimeout_.bind(this));
    this.replace_('clearTimeout', this.clearTimeout_.bind(this));
    this.replace_('setInterval', this.setInterval_.bind(this));
    this.replace_('clearInterval', this.clearInterval_.bind(this));
  }

  /**
   * Restores default behavior for scheduling callbacks.
   */
  uninstall() {
    if (this.originals_) {
      for (var key in this.originals_) {
        window[key] = this.originals_[key];
      }
    }
  }

  /**
   * Overrides a global function.
   * @param {string} functionName The name of the function.
   * @param {!Function} replacementFunction The function override.
   * @private
   */
  replace_(functionName, replacementFunction) {
    this.originals_[functionName] = window[functionName];
    window[functionName] = replacementFunction;
  }

  /**
   * Creates a virtual timer.
   * @param {!Function} callback The callback function.
   * @param {number} delayInMs The virtual delay in milliseconds.
   * @param {boolean} repeats Indicates if the timer repeats.
   * @return {number} Idetifier for the timer.
   * @private
   */
  createTimer_(callback, delayInMs, repeats) {
    var key = this.nextTimerKey_++;
    var task =
        {callback: callback, delay: delayInMs, key: key, repeats: repeats};
    this.timers_[key] = task;
    this.scheduleTask_(task);
    return key;
  }

  /**
   * Schedules a callback for execution after a virtual time delay. The tasks
   * are sorted in descending order of time delay such that the next callback
   * to fire is at the end of the list.
   * @param {{callback: Function,
   *          delay: number,
   *          key: number,
   *          repeats: boolean}} details The timer details.
   * @private
   */
  scheduleTask_(details) {
    var key = details.key;
    var when = this.now_ + details.delay;
    var index = this.schedule_.length;
    while (index > 0 && this.schedule_[index - 1].when < when) {
      index--;
    }
    this.schedule_.splice(index, 0, {when: when, key: key});
  }

  /**
   * Override of window.setInterval.
   * @param {!Function} callback The callback function.
   * @param {number} intervalInMs The repeat interval.
   * @private
   */
  setInterval_(callback, intervalInMs) {
    return this.createTimer_(callback, intervalInMs, true);
  }

  /**
   * Override of window.clearInterval.
   * @param {number} key The ID of the interval timer returned from
   *     setInterval.
   * @private
   */
  clearInterval_(key) {
    this.timers_[key] = undefined;
  }

  /**
   * Override of window.setTimeout.
   * @param {!Function} callback The callback function.
   * @param {number} delayInMs The scheduled delay.
   * @private
   */
  setTimeout_(callback, delayInMs) {
    return this.createTimer_(callback, delayInMs, false);
  }

  /**
   * Override of window.clearTimeout.
   * @param {number} key The ID of the schedule timeout callback returned
   *     from setTimeout.
   * @private
   */
  clearTimeout_(key) {
    this.timers_[key] = undefined;
  }

  /**
   * Simulates passage of time, triggering any scheduled callbacks whose timer
   * has elapsed.
   * @param {number} elapsedMs The simulated elapsed time in milliseconds.
   */
  tick(elapsedMs) {
    this.until_ += elapsedMs;
    this.fireElapsedCallbacks_();
  }

  /**
   * Triggers any callbacks that should have fired based in the simulated
   * timing.
   * @private
   */
  fireElapsedCallbacks_() {
    while (this.schedule_.length > 0) {
      var when = this.schedule_[this.schedule_.length - 1].when;
      if (when > this.until_) {
        break;
      }

      var task = this.schedule_.pop();
      var details = this.timers_[task.key];
      if (!details) {
        continue;
      }  // Cancelled task.

      this.now_ = when;
      details.callback.apply(window);
      if (details.repeats) {
        this.scheduleTask_(details);
      } else {
        this.clearTimeout_(details.key);
      }
    }
    this.now_ = this.until_;
  }
}
