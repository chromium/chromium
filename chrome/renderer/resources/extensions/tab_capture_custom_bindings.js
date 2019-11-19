// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Tab Capture API.

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  function proxyToGetUserMedia(name, request, callback, response) {
    if (!callback)
      return;

    if (!response) {
      // When the response is missing, runtime.lastError has already been set.
      // See chrome/browser/extensions/api/tab_capture/tab_capture_api.cc.
      callback(null);
      return;
    }

    // Convenience function for processing webkitGetUserMedia() error objects to
    // provide runtime.lastError messages for the tab capture API.
    function getErrorMessage(error, fallbackMessage) {
      if (!error || (typeof error.message != 'string'))
        return fallbackMessage;
      return error.message.replace(/(navigator\.)?(webkit)?GetUserMedia/gi,
                                   name);
    }

    var options = {};
    if (response.audioConstraints)
      options.audio = response.audioConstraints;
    if (response.videoConstraints)
      options.video = response.videoConstraints;
    try {
      navigator.webkitGetUserMedia(
          options,
          function onSuccess(media_stream) {
            callback(media_stream);
          },
          function onError(error) {
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

  apiFunctions.setCustomCallback('capture', proxyToGetUserMedia);
  apiFunctions.setCustomCallback('captureOffscreenTab', proxyToGetUserMedia);
});
