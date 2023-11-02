/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The scripts in this file will be injected to the http responses when
 * simulating network via Web Page Replay for Chrome Endure tests.
 *
 * If you need to modify this scripts, make sure that you use the same version
 * of this scripts in both record and replay mode.
 *
 * TODO(fdeng):
 * This file is adapted from deterministic.js in Web Page Replay project.
 * http://code.google.com/p/web-page-replay/source/browse/trunk/deterministic.js
 * The value of time_seed is modified to a date far in the future.
 * This is a workaround for Endure tests for Google apps like Gmail.
 * The side effect of a future date is unknown and needs future investigation.
 * A better way to go is to revise the time_seed to
 * current time each time we record and use the revised scripts for replay.
 * This can be achieved by modifying Web Page Replay to automatically
 * revise and save scripts in the archive in record mode and read it
 * from the archive in replay mode.
 */
(function () {
  var orig_date = Date;
  var random_count = 0;
  var date_count = 0;
  var random_seed = 0.462;
  var time_seed = 3204251968254; // Changed from default value 1204251968254
  var random_count_threshold = 25;
  var date_count_threshold = 25;
  Math.random = function() {
    random_count++;
    if (random_count > random_count_threshold) {
     random_seed += 0.1;
     random_count = 1;
    }
    return (random_seed % 1);
  };
  Date = function() {
    if (this instanceof Date) {
      date_count++;
      if (date_count > date_count_threshold) {
        time_seed += 50;
        date_count = 1;
      }
      switch (arguments.length) {
      case 0: return new orig_date(time_seed);
      case 1: return new orig_date(arguments[0]);
      default: return new orig_date(arguments[0], arguments[1],
         arguments.length >= 3 ? arguments[2] : 1,
         arguments.length >= 4 ? arguments[3] : 0,
         arguments.length >= 5 ? arguments[4] : 0,
         arguments.length >= 6 ? arguments[5] : 0,
         arguments.length >= 7 ? arguments[6] : 0);
      }
    }
    return new Date().toString();
  };
  Date.__proto__ = orig_date;
  Date.prototype.constructor = Date;
  orig_date.now = function() {
    return new Date().getTime();
  };
})();
