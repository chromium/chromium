// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MockTimer} from './mock_timer.js';

let mockTimer;

/**
 * Counter class for tallying the number if times a calback is triggered.
 * @constructor
 */
function ClickCounter() {}

ClickCounter.prototype = {
  /**
   * Nubmer of times the callback was triggered.
   * @type{number}
   * @private
   */
  clickCount_: 0,

  /** Increments click count */
  tick: function() {
    this.clickCount_++;
  },

  /**
   * Creates a callback function that tracks the number of calls.
   * @return {!Function}
   */
  createCallback: function() {
    const self = this;
    return function() {
      self.tick();
    };
  },

  /**
   * Nubmer of times the callback was triggered.
   * @type {number}
   */
  get value() {
    return this.clickCount_;
  },
};

function setUp() {
  mockTimer = new MockTimer();
  mockTimer.install();
}

function tearDown() {
  mockTimer.uninstall();
}

function testSetTimeout() {
  const counter = new ClickCounter();
  window.setTimeout(counter.createCallback(), 100);
  assertEquals(0, counter.value);
  mockTimer.tick(50);
  assertEquals(0, counter.value);
  mockTimer.tick(100);
  assertEquals(1, counter.value);
  mockTimer.tick(100);
  assertEquals(1, counter.value);
}

function testClearTimeout() {
  const counter = new ClickCounter();
  const t = window.setTimeout(counter.createCallback(), 100);

  // Verify that clearing a timeout before the elapsed time does not trigger
  // the callback.
  window.clearTimeout(t);
  mockTimer.tick(200);
  assertEquals(0, counter.value);
}

function testSetAndClearInterval() {
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
}

function testInterleavedTimers() {
  let results = '';
  const createCallback = function(response) {
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
}

Object.assign(window, {
  setUp,
  tearDown,
  testSetTimeout,
  testClearTimeout,
  testSetAndClearInterval,
  testInterleavedTimers,
});
