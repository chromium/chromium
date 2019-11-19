// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Checks for an extension error that occurred during the asynchronous call.
 * If an error occurs, will invoke the error callback and throw an exception.
 *
 * @param {function(!Error)} errCallback The callback to invoke for error
 *     reporting.
 */
function checkForExtensionError(errCallback) {
  if (typeof(chrome.extension.lastError) != 'undefined') {
    var error = new Error(chrome.extension.lastError.message);
    errCallback(error);
    throw error;
  }
}

/**
 * Launches an app with the specified id.
 *
 * @param {string} id The ID of the app to launch.
 * @param {function()} callback Invoked when the launch event is complete.
 * @param {function(!Error)} errCallback The callback to invoke for error
 *     reporting.
 */
function launchApp(id, callback, errCallback) {
  chrome.management.launchApp(id, function() {
    checkForExtensionError(errCallback);
    callback();
  });
}
