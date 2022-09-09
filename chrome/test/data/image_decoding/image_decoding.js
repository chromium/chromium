// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run benchmark for at least this many milliseconds.
var minTotalTimeMs = 10 * 1000;
// Run benchmark at least this many times.
var minIterations = 5;
// Discard results for this many milliseconds after startup.
var warmUpTimeMs = 1000;
var benchmarkStartTime;
var loadStartTimeMs;
var loadingTimesMs = [];
var isDone = false;
var img;

var now = (function() {
  if (window.performance)
    return (window.performance.now ||
            window.performance.webkitNow).bind(window.performance);
  return Date.now.bind(window);
})();

getImageFormat = function() {
  if (document.location.search) {
    return document.location.search.substr(1);
  }
  return "jpg";
}

prepareImageElement = function() {
  img = document.createElement('img');
  // Scale the image down to the display size to make sure the entire image is
  // decoded. If the image is much larger than the display, some browsers might
  // only decode the visible portion of the image, which would skew the results
  // of this test.
  img.setAttribute('width', '100%');
  img.setAttribute('height', '100%');
  document.body.appendChild(img);
  console.log("Running benchmark for at least " + minTotalTimeMs +
              " ms and at least " + minIterations + " times.");
  document.getElementById('status').innerHTML = "Benchmark running.";
  setStatus(getImageFormat().toUpperCase() + " benchmark running.");
  benchmarkStartTimeMs = now();
}

setStatus = function(status) {
  document.getElementById('status').innerHTML = status;
}

runBenchmark = function() {
  setStatus("Preparing benchmark.");
  prepareImageElement();
  startLoadingImage();
}

var iteration = (new Date).getTime();

startLoadingImage = function() {
  img.style.display = 'none';
  img.setAttribute('onload', '');
  img.setAttribute('src', '');
  img.addEventListener('load', onImageLoaded);

  // Setting 'src' and 'display' above causes a repaint. Let it finish before
  // loading a new image. Ensures loading and painting don't overlap.
  requestAnimationFrame(function() {
    loadStartTimeMs = now();
    // Use a unique URL for each test iteration to work around image caching.
    img.setAttribute('src', 'droids.' + getImageFormat() + '?' + iteration);
    iteration += 1;
  });
}

var requestAnimationFrame = (function() {
  return window.requestAnimationFrame       ||
         window.webkitRequestAnimationFrame ||
         window.mozRequestAnimationFrame    ||
         window.oRequestAnimationFrame      ||
         window.msRequestAnimationFrame     ||
         function(callback) {
           window.setTimeout(callback, 1000 / 60);
         };
})().bind(window);

onImageLoaded = function() {
  var nowMs = now();
  var loadingTimeMs = nowMs - loadStartTimeMs;
  if (nowMs - benchmarkStartTimeMs >= warmUpTimeMs) {
    loadingTimesMs.push(loadingTimeMs);
  }
  if (nowMs - benchmarkStartTimeMs < minTotalTimeMs ||
      loadingTimesMs.length < minIterations) {
    // Repaint happens right after the image is loaded. Make sure painting
    // is completed before making the image visible. Setting the image to
    // visible will start decoding. After decoding and painting are completed
    // we'll start next test iteration.
    // Double rAF is needed here otherwise the image is barely visible.
    requestAnimationFrame(function() {
      img.style.display = '';
      requestAnimationFrame(startLoadingImage);
    });
  } else {
    isDone = true;
    console.log("loadingTimes: " + loadingTimesMs);
    setStatus(getImageFormat().toUpperCase() + " benchmark finished: " +
              loadingTimesMs);
  }
}

averageLoadingTimeMs = function() {
  if (!loadingTimesMs.length)
    return 0;
  var total = 0;
  for (var i = 0; i < loadingTimesMs.length; ++i) {
    total += loadingTimesMs[i];
  }
  return total / loadingTimesMs.length;
}
