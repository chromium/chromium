// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mockTimer;

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
    var self = this;
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
  }
};

function setUp() {
  mockTimer = new MockTimer();
  mockTimer.install();
}

function tearDown() {
  mockTimer.uninstall();
}

function testSetTimeout() {
  var counter = new ClickCounter();
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
  var counter = new ClickCounter();
  var t = window.setTimeout(counter.createCallback(), 100);

  // Verify that clearing a timeout before the elapsed time does not trigger
  // the callback.
  window.clearTimeout(t);
  mockTimer.tick(200);
  assertEquals(0, counter.value);
}

function testSetAndClearInterval() {
  var counter = new ClickCounter();
  var t = window.setInterval(counter.createCallback(), 100);

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
  var results = '';
  var createCallback = function(response) {
    var label = response;
    return function() {
      results = results + label;
    };
  };

  // Verify callbacks are properly interleaved.
  var t1 = window.setInterval(createCallback('A'), 7);
  var t2 = window.setInterval(createCallback('B'), 13);
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
