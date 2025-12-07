// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageCapture API.

const handleUncaughtException = require('uncaught_exception_handler').handle;
const pageCaptureNatives = requireNative('page_capture');
const CreateBlob = pageCaptureNatives.CreateBlob;
const SendResponseAck = pageCaptureNatives.SendResponseAck;

apiBridge.registerCustomHook(function(bindingsAPI) {
  const apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('saveAsMHTML', function(callback, response) {
    let requestId;
    if (response) {
      requestId = response.requestId;
      response = CreateBlob(response.mhtmlFilePath, response.mhtmlFileLength);
    }

    try {
      callback(response);
    } catch (e) {
      handleUncaughtException(
          'Error in chrome.pageCapture.saveAsMHTML callback', e);
    } finally {
      if (requestId) {
        // If we received a blob, notify the browser. Now that the blob is
        // referenced from JavaScript, the browser can drop its reference to
        // it.
        SendResponseAck(requestId);
      }
    }
  });
});
