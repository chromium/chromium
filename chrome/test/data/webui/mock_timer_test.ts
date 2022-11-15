// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from './chai_assert.js';

import {MockTimer} from './mock_timer.js';

suite('EventTargetModuleTest', () => {
  let mockTimer: MockTimer;

  class ClickCounter {
    /** Number of times the callback was triggered */
    private clickCount_: number = 0;

    /** Increments click count */
    tick() {
      this.clickCount_++;
    }

    /**
     * Creates a callback function that tracks the number of calls.
     */
    createCallback(): Function {
      const self = this;
      return function() {
        self.tick();
      };
    }

    /**
     * Number of times the callback was triggered.
     */
    get value(): number {
      return this.clickCount_;
    }
  }

  setup(function() {
    mockTimer = new MockTimer();
    mockTimer.install();
  });

  teardown(function() {
    mockTimer.uninstall();
  });

  test('SetTimeout', function() {
    const counter = new ClickCounter();
    window.setTimeout(counter.createCallback(), 100);
    assertEquals(0, counter.value);
    mockTimer.tick(50);
    assertEquals(0, counter.value);
    mockTimer.tick(100);
    assertEquals(1, counter.value);
    mockTimer.tick(100);
    assertEquals(1, counter.value);
  });

  test('ClearTimeout', function() {
    const counter = new ClickCounter();
    const t = window.setTimeout(counter.createCallback(), 100);

    // Verify that clearing a timeout before the elapsed time does not trigger
    // the callback.
    window.clearTimeout(t);
    mockTimer.tick(200);
    assertEquals(0, counter.value);
  });

  test('SetAndClearInterval', function() {
    const counter = new ClickCounter();
    const t = window.setInterval(counter.createCallback(), 100);

    // Verify that callback doesn't fire before elapsed interval.
    assertEquals(0, counter.value);
    mockTimer.tick(50);

    // Verify that each elapsed time interval advances the count by 1.
    assertEquals(0, counter.value);
    mockTimer.tick(100);
    assertEquals(1, counter.value);
    mockTimer.tick(100);
    assertEquals(2, counter.value);
    mockTimer.tick(100);
    assertEquals(3, counter.value);

    // Verify that callbacks stop firing after timer is cleared.
    window.clearInterval(t);
    mockTimer.tick(100);
    assertEquals(3, counter.value);
  });

  test('InterleavedTimers', function() {
    let results = '';
    const createCallback = function(response: string) {
      const label = response;
      return function() {
        results = results + label;
      };
    };

    // Verify callbacks are properly interleaved.
    const t1 = window.setInterval(createCallback('A'), 7);
    const t2 = window.setInterval(createCallback('B'), 13);
    mockTimer.tick(30);
    assertEquals('ABAABA', results);
    mockTimer.tick(30);
    assertEquals('ABAABAABAABA', results);

    window.clearInterval(t1);
    window.setTimeout(createCallback('C'), 11);
    mockTimer.tick(30);
    assertEquals('ABAABAABAABABCB', results);

    window.clearInterval(t2);
    mockTimer.tick(30);
    assertEquals('ABAABAABAABABCB', results);
  });
});
