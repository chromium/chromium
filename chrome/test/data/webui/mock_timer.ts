// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Overrides timeout and interval callbacks to mock timing behavior. */
interface Timer {
  callback: Function;
  delay: number;
  key: number;
  repeats: boolean;
}

interface ScheduledTask {
  when: number;
  key: number;
}

type WindowObject = Window&{[key: string]: any};

export class MockTimer {
  /** Default versions of the timing functions. */
  private originals_: {[key: string]: Function} = {};

  /**
   * Key to assign on the next creation of a scheduled timer. Each call to
   * setTimeout or setInterval returns a unique key that can be used for
   * clearing the timer.
   */
  private nextTimerKey_: number = 1;

  /** Details for active timers. */
  private timers_: Array<(Timer | undefined)> = [];

  /** List of scheduled tasks. */
  private schedule_: ScheduledTask[] = [];

  /** Virtual elapsed time in milliseconds. */
  private now_: number = 0;

  /**
   * Used to control when scheduled callbacks fire.  Calling the 'tick' method
   * inflates this parameter and triggers callbacks.
   */
  private until_: number = 0;

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
      for (const key in this.originals_) {
        (window as WindowObject)[key] = this.originals_[key];
      }
    }
  }

  /**
   * Overrides a global function.
   * @param functionName The name of the function.
   * @param replacementFunction The function override.
   */
  private replace_(functionName: string, replacementFunction: Function) {
    this.originals_[functionName] = (window as WindowObject)[functionName];
    (window as WindowObject)[functionName] = replacementFunction;
  }

  /**
   * Creates a virtual timer.
   * @param callback The callback function.
   * @param delayInMs The virtual delay in milliseconds.
   * @param repeats Indicates if the timer repeats.
   * @return Identifier for the timer.
   */
  private createTimer_(callback: Function, delayInMs: number, repeats: boolean):
      number {
    const key = this.nextTimerKey_++;
    const task =
        {callback: callback, delay: delayInMs, key: key, repeats: repeats};
    this.timers_[key] = task;
    this.scheduleTask_(task);
    return key;
  }

  /**
   * Schedules a callback for execution after a virtual time delay. The tasks
   * are sorted in descending order of time delay such that the next callback
   * to fire is at the end of the list.
   * @param details The timer details.
   */
  private scheduleTask_(details: Timer) {
    const key = details.key;
    const when = this.now_ + details.delay;
    let index = this.schedule_.length;
    while (index > 0 && this.schedule_[index - 1]!.when < when) {
      index--;
    }
    this.schedule_.splice(index, 0, {when: when, key: key});
  }

  /**
   * Override of window.setInterval.
   * @param callback The callback function.
   * @param intervalInMs The repeat interval.
   */
  private setInterval_(callback: Function, intervalInMs: number) {
    return this.createTimer_(callback, intervalInMs, true);
  }

  /**
   * Override of window.clearInterval.
   * @param key The ID of the interval timer returned from setInterval.
   */
  private clearInterval_(key: number) {
    this.timers_[key] = undefined;
  }

  /**
   * Override of window.setTimeout.
   * @param callback The callback function.
   * @param delayInMs The scheduled delay.
   */
  private setTimeout_(callback: Function, delayInMs: number) {
    return this.createTimer_(callback, delayInMs, false);
  }

  /**
   * Override of window.clearTimeout.
   * @param key The ID of the schedule timeout callback returned
   *     from setTimeout.
   */
  private clearTimeout_(key: number) {
    this.timers_[key] = undefined;
  }

  /**
   * Simulates passage of time, triggering any scheduled callbacks whose timer
   * has elapsed.
   * @param elapsedMs The simulated elapsed time in milliseconds.
   */
  tick(elapsedMs: number) {
    this.until_ += elapsedMs;
    this.fireElapsedCallbacks_();
  }

  /**
   * Triggers any callbacks that should have fired based in the simulated
   * timing.
   */
  private fireElapsedCallbacks_() {
    while (this.schedule_.length > 0) {
      const when = this.schedule_[this.schedule_.length - 1]!.when;
      if (when > this.until_) {
        break;
      }

      const task = this.schedule_.pop();
      const details = this.timers_[task!.key];
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
