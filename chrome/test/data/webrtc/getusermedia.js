/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * See http://dev.w3.org/2011/webrtc/editor/getusermedia.html for more
 * information on getUserMedia.
 */

/**
 * Keeps track of our local stream (e.g. what our local webcam is streaming).
 * @private
 */
var gLocalStream = null;

/**
 * The MediaConstraints to use when connecting the local stream with a peer
 * connection.
 * @private
 */
var gAddStreamConstraints = {};

/**
 * String which keeps track of what happened when we requested user media.
 * @private
 */
var gRequestWebcamAndMicrophoneResult = 'not-called-yet';

/**
 * Used as a shortcut. Moved to the top of the page due to race conditions.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {string} id returns the element object specified as a parameter
 */
$ = function(id) {
  return document.getElementById(id);
};

/**
 * This function asks permission to use the webcam and mic from the browser.
 * Its callbacks will return either request-callback-granted or
 * request-callback-denied depending on the outcome. If the caller does not
 * successfully resolve the request by granting or denying, the test will hang.
 * To verify which callback was called, use obtainGetUserMediaResult().
 *
 * @param {!object} constraints Defines what to be requested, with mandatory
 *     and optional constraints defined. The contents of this parameter depends
 *     on the WebRTC version.
 */
function doGetUserMedia(constraints) {
  if (!navigator.getUserMedia) {
    returnToTest('Browser does not support WebRTC.');
    return;
  }
  debug('Requesting doGetUserMedia: constraints: ' +
        JSON.stringify(constraints, null, 0).replace(/[\r\n]/g, ''));
  navigator.getUserMedia(constraints,
               function(stream) {
                 ensureGotAllExpectedStreams_(stream, constraints);
                 getUserMediaOkCallback_(stream);
               },
               getUserMediaFailedCallback_);
}

/**
 * Must be called after calling doGetUserMedia.
 * @return {string} Returns not-called-yet if we have not yet been called back
 *     by WebRTC. Otherwise it returns either ok-got-stream or
 *     failed-with-error-x (where x is the error code from the error
 *     callback) depending on which callback got called by WebRTC.
 */
function obtainGetUserMediaResult() {
  returnToTest(gRequestWebcamAndMicrophoneResult);
  var ret = gRequestWebcamAndMicrophoneResult;
  // Reset for the next call.
  gRequestWebcamAndMicrophoneResult = 'not-called-yet';
  return ret;
}

/**
 * Stops all tracks of the last acquired local stream.
 */
function stopLocalStream() {
  if (gLocalStream == null)
    throw failTest('Tried to stop local stream, ' +
                   'but media access is not granted.');

  gLocalStream.getVideoTracks().forEach(function(track) {
    track.stop();
  });
  gLocalStream.getAudioTracks().forEach(function(track) {
    track.stop();
  });
  gLocalStream = null;
  gRequestWebcamAndMicrophoneResult = 'not-called-yet';
  returnToTest('ok-stopped');
}

// Functions callable from other JavaScript modules.

/**
 * Adds the current local media stream to a peer connection.
 * @param {RTCPeerConnection} peerConnection
 */
function addLocalStreamToPeerConnection(peerConnection) {
  if (gLocalStream == null)
    throw failTest('Tried to add local stream to peer connection, ' +
                   'but there is no stream yet.');
  try {
    peerConnection.addStream(gLocalStream, gAddStreamConstraints);
  } catch (exception) {
    throw failTest('Failed to add stream with constraints ' +
                   gAddStreamConstraints + ': ' + exception);
  }
  debug('Added local stream.');
}

/**
 * @return {string} Returns the current local stream - |gLocalStream|.
 */
function getLocalStream() {
  return gLocalStream;
}

// Internals.

/**
 * @private
 * @param {!MediaStream} stream Media stream from getUserMedia.
 * @param {!object} constraints The getUserMedia constraints object.
 */
function ensureGotAllExpectedStreams_(stream, constraints) {
  if (constraints['video'] && stream.getVideoTracks().length == 0) {
    gRequestWebcamAndMicrophoneResult = 'failed-to-get-video';
    throw ('Requested video, but did not receive a video stream from ' +
           'getUserMedia. Perhaps the machine you are running on ' +
           'does not have a webcam.');
  }
  if (constraints['audio'] && stream.getAudioTracks().length == 0) {
    gRequestWebcamAndMicrophoneResult = 'failed-to-get-audio';
    throw ('Requested audio, but did not receive an audio stream ' +
           'from getUserMedia. Perhaps the machine you are running ' +
           'on does not have audio devices.');
  }
}

/**
 * @private
 * @param {MediaStream} stream Media stream.
 */
function getUserMediaOkCallback_(stream) {
  gLocalStream = stream;
  gRequestWebcamAndMicrophoneResult = 'ok-got-stream';

  $('local-view').srcObject = stream;

  returnToTest('request-callback-granted');
}

/**
 * @private
 * @param {NavigatorUserMediaError} error Error containing details.
 */
function getUserMediaFailedCallback_(error) {
  // Translate from the old error to the new. Remove when rename fully deployed.
  var errorName = error.name;

  debug('GetUserMedia FAILED: Maybe the camera is in use by another process?');
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + errorName;
  debug(gRequestWebcamAndMicrophoneResult);

  returnToTest('request-callback-denied');
}

function openDesktopMediaStream() {
  window.addEventListener('message', function(event) {
    // Only trigger if streamId is present (callback from, not to, extension).
    if (typeof event.data.streamId !== 'undefined') {
      returnToTest(event.data.streamId);
    }
  });

  window.postMessage({desktopSourceTypes: ['window', 'screen']}, '*');
}
