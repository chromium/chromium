// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Perform various "gestures" and calculate average frame rate.
 * "Gestures" are recorded scrolling behaviors in terms of time (ms) and
 * absolute positions.
 *
 * How to run a single gesture:
 *  1) Open a webpage to test (must include this javascript).
 *  2) Type "__start('name_of_gesture')" in the javascript console.
 *  3) Wait for gesture to finish.
 *  4) Type "__calc_results()" in the console to see the test results.
 *
 * How to run all gestures:
 *  1) Open a webpage to test (must include this javascript).
 *  2) Type "__start_all()" in the javascript console.
 *  3) Wait for all gestures to finish.
 *  4) Type "__calc_results_total()" in the console to see the test results.
 *
 * How to record a new gesture:
 *  1) Open a webpage to record from (must include this javascript).
 *  2) Type "__start_recording()" in the javascript console.
 *  3) Perform any gestures you wish to record.
 *  4) Type "__stop()" in the javascript console.
 *  5) Copy the output from "JSON.stringify(__recording)" in the console.
 *  6) Paste the output in this file as a new member of __gestures.
 *  7) Copy the formatting from other gestures.
 *    Example:
 *      new_gesture_name: [
 *        {"time_ms":1, "y":0},
 *        ... pasted output ...
 *      ],
 */

var __initialized = true;
var __running = false;
var __running_all = false;
var __old_title = "";
var __raf_is_live = false;
var __raf;

var __t_start;
var __t_last;
var __t_est;
var __t_frame_intervals;

var __queued_gesture_functions;
var __results;

var __recording = [];
var __advance_gesture;

// This flag indicates whether the test page contains an animation loop
// For more on testing animated pages, see head_animation.js
var __animation = false;

var __gesture_library = {
  init: [
    {"time_ms":1, "y":0},
    {"time_ms":5, "y":10}
  ],
  stationary: [
    {"time_ms":1, "y":0},
    {"time_ms":5000, "y":0}
  ],
  steady: [
    {"time_ms":1, "y":0},
    {"time_ms":500, "y":400}
  ],
  reading: [
    {"time_ms":1, "y":0},
    {"time_ms":842, "y":40},
    {"time_ms":858, "y":67},
    {"time_ms":874, "y":94},
    {"time_ms":890, "y":149},
    {"time_ms":907, "y":203},
    {"time_ms":923, "y":257},
    {"time_ms":939, "y":311},
    {"time_ms":955, "y":393},
    {"time_ms":971, "y":542},
    {"time_ms":987, "y":718},
    {"time_ms":1003, "y":949},
    {"time_ms":1033, "y":1071},
    {"time_ms":1055, "y":1288},
    {"time_ms":1074, "y":1790},
    {"time_ms":1090, "y":1898},
    {"time_ms":1106, "y":2007},
    {"time_ms":1122, "y":2129},
    {"time_ms":1138, "y":2278},
    {"time_ms":1154, "y":2346},
    {"time_ms":1170, "y":2373}
  ],
  mouse_wheel: [
    {"time_ms":1, "y":0},
    {"time_ms":163.99, "y":0},
    {"time_ms":164, "y":53},
    {"time_ms":227.99, "y":53},
    {"time_ms":228, "y":106},
    {"time_ms":259.99, "y":106},
    {"time_ms":260, "y":160},
    {"time_ms":292.99, "y":160},
    {"time_ms":292, "y":213},
    {"time_ms":307.99, "y":213},
    {"time_ms":308, "y":266},
    {"time_ms":324.99, "y":266},
    {"time_ms":325, "y":320},
    {"time_ms":340.99, "y":320},
    {"time_ms":341, "y":373},
    {"time_ms":356.99, "y":373},
    {"time_ms":357, "y":426},
    {"time_ms":372.99, "y":426},
    {"time_ms":373, "y":480},
    {"time_ms":388.99, "y":480},
    {"time_ms":389, "y":533},
    {"time_ms":404.99, "y":533},
    {"time_ms":405, "y":586},
    {"time_ms":420.99, "y":586},
    {"time_ms":421, "y":639},
    {"time_ms":437.99, "y":639}
  ],
  mac_fling: [
    {"time_ms":1, "y":0},
    {"time_ms":212, "y":1},
    {"time_ms":228, "y":7},
    {"time_ms":261, "y":59},
    {"time_ms":277, "y":81},
    {"time_ms":293, "y":225},
    {"time_ms":309, "y":305},
    {"time_ms":325, "y":377},
    {"time_ms":342, "y":441},
    {"time_ms":358, "y":502},
    {"time_ms":374, "y":559},
    {"time_ms":391, "y":640},
    {"time_ms":408, "y":691},
    {"time_ms":424, "y":739},
    {"time_ms":441, "y":784},
    {"time_ms":458, "y":827},
    {"time_ms":523, "y":926},
    {"time_ms":540, "y":955},
    {"time_ms":560, "y":1007},
    {"time_ms":577, "y":1018},
    {"time_ms":593, "y":1040},
    {"time_ms":611, "y":1061},
    {"time_ms":627, "y":1080},
    {"time_ms":643, "y":1099},
    {"time_ms":659, "y":1115},
    {"time_ms":677, "y":1146},
    {"time_ms":693, "y":1160},
    {"time_ms":727, "y":1173},
    {"time_ms":743, "y":1190},
    {"time_ms":759, "y":1201},
    {"time_ms":776, "y":1210},
    {"time_ms":792, "y":1219},
    {"time_ms":809, "y":1228},
    {"time_ms":826, "y":1236},
    {"time_ms":843, "y":1243},
    {"time_ms":859, "y":1256},
    {"time_ms":876, "y":1262},
    {"time_ms":893, "y":1268},
    {"time_ms":911, "y":1273},
    {"time_ms":941, "y":1275},
    {"time_ms":958, "y":1282},
    {"time_ms":976, "y":1288},
    {"time_ms":993, "y":1291},
    {"time_ms":1022, "y":1294},
    {"time_ms":1055, "y":1302}
  ],
};

// Stretch the duration of a gesture by a given factor
function __gesture_stretch(gesture, stretch_factor) {
  // clone the gesture
  var new_gesture = structuredClone(gesture);
  for (var i = 0; i < new_gesture.length; ++i) {
    new_gesture[i].time_ms *= stretch_factor;
  }
  return new_gesture;
}

// Gesture set to use for testing, initialized with default gesture set.
// Redefine in test file to use a different set of gestures.
var __gestures = {
  steady: __gesture_library["steady"],
};

function __init_stats() {
  __t_last = undefined;
  __t_est = undefined;
  __t_frame_intervals = [];
}
__init_stats();

var __cur_chrome_interval;
function __init_time() {
  if (chrome.Interval) {
    __cur_chrome_interval = new chrome.Interval();
    __cur_chrome_interval.start();
  }
}
__init_time();

function __get_time() {
  if (__cur_chrome_interval)
    return __cur_chrome_interval.microseconds() / 1000;
  return new Date().getTime();
}

function __init_raf() {
  if ("requestAnimationFrame" in window)
    __raf = requestAnimationFrame;
  else if ("webkitRequestAnimationFrame" in window)
    __raf = webkitRequestAnimationFrame;
  else if ("mozRequestAnimationFrame" in window)
    __raf = mozRequestAnimationFrame;
  else if ("oRequestAnimationFrame" in window)
    __raf = oRequestAnimationFrame;
  else if ("msRequestAnimationFrame" in window)
    __raf = msRequestAnimationFrame;
  else
    // No raf implementation available, fake it with 16ms timeouts
    __raf = function(callback, element) {
      setTimeout(callback, 16);
    };
}
__init_raf();

function __calc_results() {
  var M = 0.0;
  var N = __t_frame_intervals.length;

  for (var i = 0; i < N; i++)
    M += __t_frame_intervals[i];
  M = M / N;

  var V = 0.0;
  for (var i = 0; i < N; i++) {
    var v = __t_frame_intervals[i] - M;
    V += v * v;
  }

  var S = Math.sqrt(V / (N - 1));

  var R = new Object();
  R.mean = M;
  R.sigma = S;
  return R;
}

function __calc_results_total() {
  if (!__results) {
    return {};
  }
  var size = __results.gestures.length;
  var mean = 0;
  var variance = 0;

  // Remove any intial caching test(s).
  while (__results.means.length != size) {
    __results.means.shift();
    __results.sigmas.shift();
  }
  for (var i = 0; i < size; i++) {
    mean += __results.means[i];
    variance += __results.sigmas[i] * __results.sigmas[i];
  }
  mean /= size;
  variance /= size;
  var sigma = Math.sqrt(variance);

  var results = new Object();
  // GTest expects a comma-separated string for lists.
  results.gestures = __results.gestures.join(",");
  results.means = __results.means.join(",");
  results.sigmas = __results.sigmas.join(",");
  results.mean = mean;
  results.sigma = sigma;
  return results;
}

function __record_frame_time() {
  var t_now = __get_time();
  if (window.__t_last) {
    var t_delta = t_now - __t_last;
    if (window.__t_est) {
      __t_est = (0.1 * __t_est) + (0.9 * t_delta);  // low-pass filter
    } else {
      __t_est = t_delta;
    }

    __t_frame_intervals.push(t_delta);

    var fps = 1000.0 / __t_est;
    document.title = "FPS: " + fps;
  }
  __t_last = t_now;
}

// Returns true if a recorded gesture movement occured.
function __advance_gesture_recording() {
  var y = document.body.scrollTop;
  // Only add a gesture if the scroll position changes.
  if (__recording.length == 0 || y != __recording[__recording.length - 1].y) {
    var time_ms = __get_time() - __t_start;
    __recording.push({ time_ms: time_ms, y: y });
    return true;
  }
  return false;
}

function __scroll_window_to(y) {
  // Scrolls a window to a new location using window.scrollBy, but avoids
  // window.scrollTo because of potential animation that may cause. This tracks
  // the current scrollTop position to avoid forcing layout.
  //
  // Returns true if the window actually scrolled.
  var yfloor = Math.floor(y);
  if (window.__scrolledTo === undefined) {
    window.scrollBy(0, yfloor);
    window.__scrolledTo = yfloor;
    return true;
  }

  var delta = yfloor - window.__scrolledTo;
  if (delta == 0)
    return false;
  window.scrollBy(0, delta);
  window.__scrolledTo = yfloor;
  return true;
}

// Returns true if a gesture movement occured.
function __create_gesture_function(gestures) {
  var i0 = 0;
  return function() {
    if (i0 >= gestures.length) {
      __stop();
      return false;
    }
    var time_cur = __get_time() - __t_start;
    if (time_cur <= gestures[i0].time_ms)
      return false;

    // Skip any keyframes that we missed
    for (i0; i0 < gestures.length && gestures[i0].time_ms < time_cur; ++i0);

    // This loop overshoots by 1, so move back in time by 1
    i0--;
    var i1 = i0 + 1;

    if (i1 < gestures.length) {
      // If i1 exists, interpolate between i0 and i1 y based on time_cur -
      // gestures[i0].time_ms
      var time_since_i0_start = time_cur - gestures[i0].time_ms;
      var time_between_i0_and_i1 = gestures[i1].time_ms -
          gestures[i0].time_ms;
      var percent_into_frame = time_since_i0_start / time_between_i0_and_i1;
      var y_diff_between_i0_and_i1 = gestures[i1].y - gestures[i0].y;
      var interpolated_y = gestures[i0].y +
          (percent_into_frame * y_diff_between_i0_and_i1);

      // Skip all gestures that occured within the same time interval.
      return __scroll_window_to(interpolated_y);
    } else {
      // Only generate a frame if we are at the end of the animation.
      __stop();
      return false;
    }
  };
}

function __create_repeating_gesture_function(gestures) {
  var dtime_ms = gestures[gestures.length - 1].time_ms;
  var dy = gestures[gestures.length - 1].y;

  // Do not repeat gestures that do not change the Y position.
  if (dy == 0)
    return __create_gesture_function(gestures);

  // Repeat gestures until a scroll goes out of bounds.
  // Note: modifies a copy of the given gestures array.
  gestures = gestures.slice(0);
  for (var i = 0, y = 0; y >= 0 && y < document.body.scrollHeight; ++i) {
    y = gestures[i].y + dy;
    gestures.push({
      time_ms: gestures[i].time_ms + dtime_ms,
      y: y,
    });
  }
  return __create_gesture_function(gestures);
}

function __sched_update() {
  __raf(function() {
    __raf_is_live = true;
    // In case __raf falls back to using setTimeout, we must schedule the next
    // update before rendering the current update to help maintain the
    // regularity of update frame_intervals.
    __sched_update();
    if (__running) {
      // Only update the FPS if a gesture movement occurs. Otherwise, the frame
      // rate average becomes inaccurate after any pause.
      if (__advance_gesture())
        __record_frame_time();
      else
        __t_last = __get_time();
    }
  }, document.body);
}

function __start_recording() {
  __start(__advance_gesture_recording);
}

function __make_body_composited() {
  document.body.style.transform = "translateZ(0)";
}

function __start(gesture_function) {
  if (__running)
    return;
  // Attempt to create a gesture function from a string name.
  if (typeof gesture_function == "string") {
    if (!__gestures[gesture_function]) {
      if (!__gesture_library[gesture_function])
        throw new Error("Unrecognized gesture name");
      else
        gesture_function = __create_repeating_gesture_function(
                            __gesture_library[gesture_function]);
    }
    else
      gesture_function = __create_repeating_gesture_function(
                            __gestures[gesture_function]);
  }
  else if (typeof gesture_function != "function")
    throw new Error("Argument is not a function or gesture name");

  __old_title = document.title;
  __advance_gesture = gesture_function;
  __t_start = __get_time();
  __running = true;
  if (!__raf_is_live && !__animation) {
    __sched_update();
  }
}

function __start_all() {
  __queued_gesture_functions = [];
  __results = {
    gestures: [],
    means: [],
    sigmas: [],
  };

  for (var gesture in __gestures) {
    __results.gestures.push(gesture);
    __queued_gesture_functions.push(gesture);
  }
  __running_all = true;
  // Run init gesture once to cache the webpage layout for subsequent tests.
  __start("init");
}

function __stop() {
  __running = false;
  document.title = __old_title;
  window.__scrolledTo = undefined;

  if (__running_all) {
    var results = __calc_results();
    __results.means.push(results.mean);
    __results.sigmas.push(results.sigma);

    if (__queued_gesture_functions.length > 0) {
      document.body.scrollTop = 0;
      __init_stats();
      __start(__queued_gesture_functions.shift());
    } else {
      __running_all = false;
    }
  }
}

function __reset() {
  __running = false;
  __running_all = false;
  document.title = __old_title;
  document.body.scrollTop = 0;
  __init_stats();
}

function __force_compositor() {
  document.body.style.transform = "translateZ(0)";
}
