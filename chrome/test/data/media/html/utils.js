// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Miscellaneous utility functions for HTML media tests. Loading this script
// should not modify the page in any way.
//

var QueryString = function () {
  // Allows access to query parameters on the URL; e.g., given a URL like:
  //
  //    http://<url>/my.html?test=123&bob=123
  //
  // parameters can now be accessed via QueryString.test or QueryString.bob.
  var params = {};

  // RegEx to split out values by &.
  var r = /([^&=]+)=?([^&]*)/g;

  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  function d(s) { return decodeURIComponent(s.replace(/\+/g, ' ')); }

  var match;
  while (match = r.exec(window.location.search.substring(1)))
    params[d(match[1])] = d(match[2]);

  return params;
} ();

function getCurrentTime() {
  if (window.performance.now)
    return window.performance.now();
  else
    return new Date().getTime();
}

function Timer() {
  this.start_ = 0;
  this.times_ = [];
}

Timer.prototype = {
  start: function() {
    this.start_ = getCurrentTime();
  },

  stop: function() {
    var delta = getCurrentTime() - this.start_;
    this.times_.push(delta);
    return delta;
  },

  reset: function() {
    this.start_ = 0;
    this.times_ = [];
  }
}

function GenerateUniqueURL(src) {
  var ch = src.indexOf('?') >= 0 ? '&' : '?';
  return src + ch + 't=' + (new Date()).getTime();
}