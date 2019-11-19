// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests the microphone module of Voice Search on the local NTP.
 */

/**
 * Voice Search Microphone module's object for test and setup functions.
 */
test.microphone = {};

/**
 * Utility to test code that uses timeouts.
 * @type {MockClock}
 */
test.microphone.clock = new MockClock();

/**
 * Set up the microphone DOM and test environment.
 */
test.microphone.setUp = function() {
  microphone.isLevelAnimating_ = false;
  test.microphone.clock.reset();

  setUpPage('voice-microphone-template');
  microphone.init();

  test.microphone.clock.install();
};

/**
 * Makes sure the microphone module sets up with the correct settings.
 */
test.microphone.testInitialization = function() {
  assertFalse(microphone.isLevelAnimating_);
};

/**
 * Make sure the volume level animation starts.
 */
test.microphone.testStartLevelAnimationFromInactive = function() {
  microphone.startInputAnimation();
  assertTrue(microphone.isLevelAnimating_);
};

/**
 * Make sure the level animation stops.
 */
test.microphone.testStopLevelAnimationFromActive = function() {
  microphone.startInputAnimation();
  microphone.stopInputAnimation();
  assertFalse(microphone.isLevelAnimating_);
};

/**
 * Make sure the level animation doesn't start again.
 */
test.microphone.testStartLevelAnimationFromActive = function() {
  // Start the animation.
  test.microphone.clock.setTime(1);
  microphone.startInputAnimation();
  assertTrue(microphone.isLevelAnimating_);
  assertEquals(1, test.microphone.clock.pendingTimeouts.length);
  const stepOneTimeoutId = test.microphone.clock.pendingTimeouts[0].id;

  // Try to start the animation again and observe that it hasn't restarted.
  microphone.startInputAnimation();
  assertTrue(microphone.isLevelAnimating_);
  assertEquals(1, test.microphone.clock.pendingTimeouts.length);
  assertEquals(stepOneTimeoutId, test.microphone.clock.pendingTimeouts[0].id);
};
