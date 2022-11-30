// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Tab Capture API.

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  function proxyToGetUserMedia(callback, response) {
    if (!callback)
      return;

    if (!response) {
      // When the response is missing, runtime.lastError has already been set.
      // See chrome/browser/extensions/api/tab_capture/tab_capture_api.cc.
      callback(null);
      return;
    }

    // Convenience function for processing getUserMedia() error objects to
    // provide runtime.lastError messages for the tab capture API.
    const getErrorMessage = (error, fallbackMessage) => {
      if (!error || (typeof error.message !== 'string'))
        return fallbackMessage;
      return error.message.replace('navigator.mediaDevices.getUserMedia',
                                   'tabCapture.capture');
    };

    let constraints = {};
    if (response.audioConstraints)
      constraints.audio = response.audioConstraints;
    if (response.videoConstraints)
      constraints.video = response.videoConstraints;
    try {
      navigator.mediaDevices.getUserMedia(constraints)
          .then(callback)
          .catch(error => {
              bindingUtil.runCallbackWithLastError(
                getErrorMessage(error, "Failed to start MediaStream."),
                $Function.bind(callback, null, null));
          });
    } catch (error) {
      bindingUtil.runCallbackWithLastError(
          getErrorMessage(error, "Invalid argument(s)."),
          $Function.bind(callback, null, null));
    }
  }

  bindingsAPI.apiFunctions.setCustomCallback('capture', proxyToGetUserMedia);
});
