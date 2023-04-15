/**
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file requires the functions defined in test_functions.js.

var gFingerprints = [];
var gDetectorInterval = null;

// Public interface.

/**
 * Starts capturing frames from a video tag. The algorithm will fingerprint a
 * a frame every now and then. After calling this function and running for a
 * while (at least 200 ms) you will be able to call isVideoPlaying to see if
 * we detected any video.
 *
 * @param {string} videoElementId The video element to analyze.
 * @param {int}    width The video element's width.
 * @param {int}    width The video element's height.
 *
 * @return {string} Returns ok-started to the test.
 */
//
function startDetection(videoElementId, width, height) {
  var video = document.getElementById(videoElementId);
  if (!video)
    throw failTest('Could not find video element with id ' + videoElementId);

  if (gDetectorInterval)
    throw failTest('Detector is already running.');

  var NUM_FINGERPRINTS_TO_SAVE = 5;
  var canvas = document.createElement('canvas');
  canvas.style.display = 'none';

  gFingerprints = [];
  gDetectorInterval = setInterval(function() {
    var context = canvas.getContext('2d', {willReadFrequently: true});
    if (video.videoWidth == 0)
      return;  // The video element isn't playing anything.

    captureFrame_(video, context, width, height);
    gFingerprints.push(fingerprint_(context, width, height));
    if (gFingerprints.length > NUM_FINGERPRINTS_TO_SAVE) {
      gFingerprints.shift();
    }
  }, 100);

  return logAndReturn('ok-started');
}

/**
 * Checks if we have detected any video so far.
 *
 * @return {string} video-playing if we detected video, otherwise
 *                  video-not-playing.
 */
function isVideoPlaying() {
  // Video is considered to be playing if at least one finger print has changed
  // since the oldest fingerprint. Even small blips in the pixel data will cause
  // this check to pass. We only check for rough equality though to account for
  // rounding errors.
  try {
    if (gFingerprints.length > 1) {
      if (!allElementsRoughlyEqualTo_(gFingerprints, gFingerprints[0])) {
        clearInterval(gDetectorInterval);
        gDetectorInterval = null;
        return 'video-playing';
      }
    }
  } catch (exception) {
    throw failTest('Failed to detect video: ' + exception.message);
  }
  return 'video-not-playing';
}

/**
 * Checks if the video has stopped
 *
 * @return {string} video-stopped or video-not-stopped.
 */
function isVideoStopped() {
  // Video is considered to be stopped if the last 5 fingerprints are the same.
  // We only check for rough equality though to account for rounding errors.
  if (gFingerprints.length < 5) {
    return 'video-not-stopped';
  }

  if (allElementsRoughlyEqualTo_(gFingerprints.slice(-5),
                                 gFingerprints[gFingerprints.length - 1])) {
    return 'video-stopped';
  }

  return 'video-not-stopped';
}

/**
 * Queries for the stream size (not necessarily the size at which the video tag
 * is rendered).
 *
 * @param videoElementId The video element to check.
 * @return {string} ok-<width>x<height>, e.g. ok-640x480 for VGA.
 */
function getStreamSize(videoElementId) {
  var video = document.getElementById(videoElementId);
  if (!video)
    throw failTest('Could not find video element with id ' + videoElementId);

  return logAndReturn('ok-' + video.videoWidth + 'x' + video.videoHeight);
}

// Internals.

/** @private */
function allElementsRoughlyEqualTo_(elements, element_to_compare) {
  if (elements.length == 0)
    return false;

  var PIXEL_DIFF_TOLERANCE = 100;
  for (var i = 0; i < elements.length; i++) {
    if (Math.abs(elements[i] - element_to_compare) > PIXEL_DIFF_TOLERANCE) {
      return false;
    }
  }
  return true;
}

/** @private */
function captureFrame_(video, canvasContext, width, height) {
  canvasContext.drawImage(video, 0, 0, width, height);
}

/** @private */
function fingerprint_(canvasContext, width, height) {
  var imageData = canvasContext.getImageData(0, 0, width, height);
  var pixels = imageData.data;

  var fingerprint = 0;
  for (var i = 0; i < pixels.length; i++) {
    fingerprint += pixels[i];
  }
  return fingerprint;
}
