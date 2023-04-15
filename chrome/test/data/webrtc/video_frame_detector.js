/**
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file requires the functions defined in test_functions.js.

var gNumFrames = 0;

// Public interface.

/**
 * Enables video frame callbacks for a video tag. The algorithm relies on
 * requestVideoFrameCallback.
 * After callbacks have been enabled, retrieve the current frame counter with
 * getNumVideoFrameCallbacks.
 */
function enableVideoFrameCallbacks(videoElementId) {
  const video = document.getElementById(videoElementId);
  if (!video)
    throw new Error('Could not find video element with id ' + videoElementId);
  const callback = (now, metadata) => {
    ++gNumFrames;
    video.requestVideoFrameCallback(callback);
  };
  video.requestVideoFrameCallback(callback);
  return logAndReturn('ok-started');
}

/**
 * Returns the number of frame callback invocations so far.
 */
function getNumVideoFrameCallbacks() {
  return logAndReturn(`${gNumFrames}`);
}
