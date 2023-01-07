// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  FrameRateTest harness variant for benchmarking
 * pages that contain content that is animated (e.g. canvas2d, webGL)
 *
 * How to instrument an animated web page for use with the frame rate test
 * harness:
 * 1. Include head.js followed by head_animation.js in the head of the test
 *    page.
 * 2. If the animation loop does not already use requestAnimationFrame,
 *    convert it.
 * 3. Replace calls to requestAnimationFrame with the __requestAnimationFrame
 *    function defined below
 * 4. If the test page needs to call requestAnimationFrame during an
 *    initialization phase that should not be measured by the test, then
 *    use __requestAnimationFrame_no_sampling
 */

// default gestures for animated content
__gestures = {
  none: __gesture_library["stationary"]
};

// Don't start benchmarking animated pages right away.
// wait for initialization to complete.
__initialized = false;

// Indicate to the test harness that the test page has its own
// animation loop.
__animation = true;

// Draw this many frames before starting test.
// This is a delay to allow caches and other resources to spin-up to reach a
// steady running state before benchmarking begins.
var __warmup_frames = 10;

function __did_render_frame() {
  if (__warmup_frames > 0){
    __warmup_frames--;
    return;
  }

  // Signal that page is ready to begin benchamarking
  __initialized = true;

  if (__running) {
    __advance_gesture();
    __record_frame_time();
  }
}

function __requestAnimationFrame(callback, element) {
  __did_render_frame();
  __raf(callback, element);
}

function __requestAnimationFrame_no_sampling(callback, element) {
  __raf(callback, element);
}


