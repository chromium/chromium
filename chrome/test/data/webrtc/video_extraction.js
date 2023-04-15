/**
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * FPS at which we would like to sample.
 * @private
 */
var gFps = 30;

/**
 * The duration of the all frame capture in milliseconds.
 * @private
 */
var gCaptureDuration = 0;

/**
 * The recorded video encoded in Base64.
 * @private
 */
var gVideoBase64 = '';

/**
 * Chunks of the video recorded by MediaRecorded as they become available.
 * @private
 */
var gChunks = [];

/**
 * A string to be returned to the test about the current status of capture.
 */
var gCapturingStatus = 'capturing-not-started';

/**
 * Starts the frame capturing.
 *
 * @param {!Object} The video tag from which the height and width parameters are
 *                  to be extracted.
 * @param {Number} The duration of the frame capture in seconds.
 */
function startFrameCapture(videoTag, duration) {
  var inputElement = document.getElementById('local-view');
  var width = inputElement.videoWidth;
  var height = inputElement.videoHeight;

  // |videoBitsPerSecond| is set to a large number to indicate VP8 to throw as
  // little information away as possible.
  var mediaRecorderOptions = {'videoBitsPerSecond': 4 * width * height * gFps};
  var stream = getStreamFromElement_(videoTag);
  gCaptureDuration = 1000 * duration;

  var mediaRecorder = new MediaRecorder(stream, mediaRecorderOptions);

  mediaRecorder.ondataavailable = function(recording) {
    gChunks.push(recording.data);
  }
  mediaRecorder.onstop = function() {
    var videoBlob = new Blob(gChunks, {type: "video/webm"});
    gChunks = [];
    var reader = new FileReader();
    reader.onloadend = function() {
      gVideoBase64 = reader.result.substr(reader.result.indexOf(',') + 1);
      gCapturingStatus = 'done-capturing';
      debug('done-capturing');
    }
    reader.readAsDataURL(videoBlob);
  }

  mediaRecorder.start();
  setTimeout(function() { mediaRecorder.stop(); }, gCaptureDuration);
  gCapturingStatus = 'still-capturing';
}

/**
 * Returns the video recorded by RecordMedia encoded in Base64.
 */
function getRecordedVideoAsBase64() {
  return gVideoBase64;
}

/**
 * Queries if we're done with the frame capturing yet.
 */
function doneFrameCapturing() {
  return logAndReturn(gCapturingStatus);
}

/**
 * Returns the stream from the input element to be attached to MediaRecorder.
 * @private
 */
function getStreamFromElement_(element) {
  if (element.srcObject !== undefined) {
    return element.srcObject;
  } else if (element.src !== undefined) {
    return element.src;
  } else {
    throw new Error('Error extracting stream from element.');
  }
}
