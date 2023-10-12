// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

// TODO(calamity): Remove TestTimerProxy in favor of MockTimer.
export class TestTimerProxy {
  immediatelyResolveTimeouts: boolean = true;
  private nextTimeoutId_: number = 0;
  private activeTimeouts_: Map<number, Function> = new Map();

  setTimeout(fn: Function, _delay: number): number {
    if (this.immediatelyResolveTimeouts) {
      fn();
    } else {
      this.activeTimeouts_.set(this.nextTimeoutId_, fn);
    }

    return this.nextTimeoutId_++;
  }

  clearTimeout(id: number) {
    this.activeTimeouts_.delete(id);
  }

  /**
   * Run the function associated with a timeout id and clear it from the
   * active timeouts.
   */
  runTimeoutFn(id: number) {
    const fn = this.activeTimeouts_.get(id);
    assert(fn);
    fn();
    this.clearTimeout(id);
  }

  /**
   * Returns true if a given timeout id has not been run or cleared.
   */
  hasTimeout(id: number): boolean {
    return this.activeTimeouts_.has(id);
  }
}
