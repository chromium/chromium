/**
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file requires the functions defined in test_functions.js.

var gNumVideoFrameCallbacks = 0;
var gNumFramesSinceStartDetection = 0;
var gVideoElements = {};
var gFrameCountsHistory = [];
var gDetectorInterval = null;

function getElementRVfcCount(videoElementId) {
  const state = gVideoElements[videoElementId];
  return state ? state.rVfcCount : 0;
}

function hasVideoPlayed(videoElementId) {
  if (videoElementId && gVideoElements[videoElementId]) {
    const state = gVideoElements[videoElementId];
    const video = state.element;
    if (state.rVfcCount > 0) {
      return true;
    }
    if (video.getVideoPlaybackQuality) {
      const q = video.getVideoPlaybackQuality();
      if (q && q.totalVideoFrames > state.initialQualityFrames) {
        return true;
      }
    }
    if (video.currentTime > 0 && video.readyState >= 2) {
      return true;
    }
    return false;
  }

  // Global check across all monitored elements or general state.
  if (gNumFramesSinceStartDetection > 0) {
    return true;
  }
  for (const id in gVideoElements) {
    if (hasVideoPlayed(id)) {
      return true;
    }
  }
  return false;
}

// Public interface.

/**
 * Enables video frame callbacks for a video tag. The algorithm relies on
 * requestVideoFrameCallback.
 * After callbacks have been enabled, retrieve the current frame counter with
 * getNumVideoFrameCallbacks.
 *
 * @param {string} videoElementId The video element to monitor.
 * @return {string} Returns ok-started to the test.
 */
function enableVideoFrameCallbacks(videoElementId) {
  const video = document.getElementById(videoElementId);
  if (!video) {
    throw new Error('Could not find video element with id ' + videoElementId);
  }

  let initialQuality = 0;
  if (video.getVideoPlaybackQuality) {
    const q = video.getVideoPlaybackQuality();
    if (q && typeof q.totalVideoFrames === 'number') {
      initialQuality = q.totalVideoFrames;
    }
  }

  const state = {
    element: video,
    rVfcCount: 0,
    initialQualityFrames: initialQuality,
  };
  gVideoElements[videoElementId] = state;
  gNumFramesSinceStartDetection = 0;
  gFrameCountsHistory = [];

  if (!video.dataset.hasFrameCallback) {
    video.dataset.hasFrameCallback = 'true';
    const callback = (now, metadata) => {
      ++gNumVideoFrameCallbacks;
      ++gNumFramesSinceStartDetection;
      const currentState = gVideoElements[videoElementId];
      if (currentState) {
        currentState.rVfcCount++;
      }
      video.requestVideoFrameCallback(callback);
    };
    video.requestVideoFrameCallback(callback);
  }

  if (!gDetectorInterval) {
    gDetectorInterval = setInterval(function() {
      // Record history for isVideoStopped()
      let total = gNumVideoFrameCallbacks;
      for (const id in gVideoElements) {
        const s = gVideoElements[id];
        if (s && s.element && s.element.getVideoPlaybackQuality) {
          const q = s.element.getVideoPlaybackQuality();
          if (q && typeof q.totalVideoFrames === 'number') {
            total += q.totalVideoFrames;
          }
        }
      }
      gFrameCountsHistory.push(total);
      if (gFrameCountsHistory.length > 5) {
        gFrameCountsHistory.shift();
      }
    }, 100);
  }

  return logAndReturn('ok-started');
}

/**
 * Starts detection on a video tag. Alias for enableVideoFrameCallbacks for
 * compatibility with tests using the startDetection API.
 *
 * @param {string} videoElementId The video element to analyze.
 * @param {int}    width Optional width (unused).
 * @param {int}    height Optional height (unused).
 * @return {string} Returns ok-started to the test.
 */
function startDetection(videoElementId, width, height) {
  return enableVideoFrameCallbacks(videoElementId);
}

/**
 * Returns the number of frame callback invocations so far.
 *
 * @param {string} [videoElementId] Optional video element to check.
 * @return {string} Frame count as string.
 */
function getNumVideoFrameCallbacks(videoElementId) {
  if (videoElementId && gVideoElements[videoElementId]) {
    return logAndReturn(`${getElementRVfcCount(videoElementId)}`);
  }
  return logAndReturn(`${gNumVideoFrameCallbacks}`);
}

/**
 * Checks if we have detected any video so far.
 *
 * @param {string} [videoElementId] Optional video element to check.
 * @return {string} video-playing if we detected video, otherwise
 *                  video-not-playing.
 */
function isVideoPlaying(videoElementId) {
  if (hasVideoPlayed(videoElementId)) {
    return 'video-playing';
  }
  return 'video-not-playing';
}

/**
 * Checks if the video has stopped.
 *
 * @param {string} [videoElementId] Optional video element to check.
 * @return {string} video-stopped or video-not-stopped.
 */
function isVideoStopped(videoElementId) {
  const video = videoElementId ? document.getElementById(videoElementId) : null;
  if (video && (video.paused || video.ended)) {
    return 'video-stopped';
  }

  if (gFrameCountsHistory.length < 5) {
    return 'video-not-stopped';
  }

  const last = gFrameCountsHistory[gFrameCountsHistory.length - 1];
  for (let i = 0; i < gFrameCountsHistory.length; i++) {
    if (gFrameCountsHistory[i] !== last) {
      return 'video-not-stopped';
    }
  }

  return 'video-stopped';
}

/**
 * Queries for the stream size (not necessarily the size at which the video tag
 * is rendered).
 *
 * @param {string} videoElementId The video element to check.
 * @return {string} ok-<width>x<height>, e.g. ok-640x480 for VGA.
 */
function getStreamSize(videoElementId) {
  const video = document.getElementById(videoElementId);
  if (!video) {
    throw new Error('Could not find video element with id ' + videoElementId);
  }

  return logAndReturn('ok-' + video.videoWidth + 'x' + video.videoHeight);
}
